//
// Created by wcl on 2020/11/05.
//

#include "MJPEGEncoderOpenCLImpl.h"

using namespace io;
using namespace std;
using namespace core;
using namespace core::encoder;

MJPEGEncoderOpenCLImpl::MJPEGEncoderOpenCLImpl(const Arguments &arguments)
        : AbstractMJPEGEncoder(arguments) {
    this->_device = nullptr;
    this->_context = nullptr;
    this->_program = nullptr;
    this->_clCmdQueue = nullptr;

    // generate huffmanLuminanceDC and huffmanLuminanceAC first
    generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, _huffmanLuminanceDC);
    generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, _huffmanLuminanceAC);
    // generate huffmanChrominanceDC and huffmanChrominanceAC first
    generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, _huffmanChrominanceDC);
    generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, _huffmanChrominanceAC);
    int quality = _arguments.quality;

    quality = clamp(quality, 1, 100);
    quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

    for (auto i = 0; i < 8 * 8; ++i) {
        int luminance = (DefaultQuantLuminance[ZigZagInv[i]] * quality + 50) / 100;
        int chrominance = (DefaultQuantChrominance[ZigZagInv[i]] * quality + 50) / 100;

        // clamp to 1..255
        _quantLuminance[i] = clamp(luminance, 1, 255);
        _quantChrominance[i] = clamp(chrominance, 1, 255);
    }

    for (auto i = 0; i < 8 * 8; ++i) {
        auto row = ZigZagInv[i] / 8; // same as ZigZagInv[i] >> 3
        auto column = ZigZagInv[i] % 8; // same as ZigZagInv[i] &  7
        auto factor = 1 / (AanScaleFactors[row] * AanScaleFactors[column] * 8);
        _scaledLuminance[ZigZagInv[i]] = factor / _quantLuminance[i];
        _scaledChrominance[ZigZagInv[i]] = factor / _quantChrominance[i];
    }
}

void MJPEGEncoderOpenCLImpl::encodeJpeg(
        color::RGBA *paddedData, int length,
        vector<char> &output,
        void **sharedData
) {
    auto yuvFrameBuffer = static_cast<color::YCbCr444 *>(sharedData[0]);
    transformColorSpace(paddedData, *yuvFrameBuffer, this->_cachedPaddingSize);

    _bitBuffer.init();
    writeJFIFHeader(output);

    writeQuantizationTable(output, _quantLuminance, _quantChrominance);

    // write infos: SOF0 - start of frame
    writeImageInfos(output);

    writeHuffmanTable(output);

    writeScanInfo(output);

    // precompute JPEG codewords for quantized DCT
    BitCode codewordsArray[
            2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
    BitCode *codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
    uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
    int32_t mask = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
    for (int16_t value = 1; value < CodeWordLimit; value++) {
        // numBits = position of highest set bit (ignoring the sign)
        // mask    = (2^numBits) - 1
        if (value > mask) // one more bit ?
        {
            numBits++;
            mask = (mask << 1) | 1; // append a set bit
        }
        codewords[-value] = BitCode(mask - value,
                                    numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
        codewords[+value] = BitCode(value, numBits);
    }

    // average color of the previous MCU
    int16_t lastYDC = 0, lastCbDC = 0, lastCrDC = 0;
    auto maxWidth = (this->_cachedPaddingSize).width - 1;
    auto maxHeight = (this->_cachedPaddingSize).height - 1;
    float Y[8][8], Cb[8][8], Cr[8][8];
    for (auto mcuY = 0; mcuY < (this->_cachedPaddingSize).height; mcuY += 8) { // each step is either 8 or 16 (=mcuSize)
        for (auto mcuX = 0; mcuX < (this->_cachedPaddingSize).width; mcuX += 8) {
            for (auto deltaY = 0; deltaY < 8; ++deltaY) {
                auto column = mcuX;
                auto row = (mcuY + deltaY > maxHeight) ? maxHeight : mcuY + deltaY;
                for (auto deltaX = 0; deltaX < 8; ++deltaX) {
                    auto pixelPos = row * (maxWidth + 1) + column;
                    column = (column < maxWidth) ? column + 1 : column;

                    Y[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getYChannel()[pixelPos]) - 128;
                    Cb[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCbChannel()[pixelPos]) - 128;
                    Cr[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCrChannel()[pixelPos]) - 128;

                }
            }
            lastYDC = encodeBlock(output, Y, _scaledLuminance, lastYDC, _huffmanLuminanceDC, _huffmanLuminanceAC,
                                  codewords);
            lastCbDC = encodeBlock(output, Cb, _scaledChrominance, lastCbDC, _huffmanChrominanceDC,
                                   _huffmanChrominanceAC, codewords);
            lastCrDC = encodeBlock(output, Cr, _scaledChrominance, lastCrDC, _huffmanChrominanceDC,
                                   _huffmanChrominanceAC, codewords);

        } // end mcuX
    } // end mcuY

    writeBitCode(output, BitCode(0x7F, 7), _bitBuffer);

    output.push_back(0xFF);
    output.push_back(0xD9);
}

void MJPEGEncoderOpenCLImpl::start() {
    this->bootstrap();

    RawVideoReader videoReader(_arguments.input, _arguments.size);
    auto totalFrames = videoReader.getTotalFrames();
    const auto totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

    size_t paddedRgbFrameSize = totalPixels * RawVideoReader::PIXEL_BYTES;

    auto buffer = new char[videoReader.getPerFrameSize()];
    auto paddedBuffer = new char[paddedRgbFrameSize];
    auto paddedRgbaPtr = (color::RGBA *) paddedBuffer;
    AVIOutputStream aviOutputStream(_arguments.output);

    (&aviOutputStream)
            ->setFps(_arguments.fps)
            ->setSize(_arguments.size)
            ->setTotalFrames(videoReader.getTotalFrames());

    color::YCbCr444 yuvFrameBuffer(this->_cachedPaddingSize);

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer;

    aviOutputStream.start();

    void *passData[] = {
            &yuvFrameBuffer,
            nullptr,
    };

    auto totalSeconds = Utils::getCurrentTimestamp(
            videoReader.getTotalFrames(), videoReader.getTotalFrames(),
            _arguments.fps
    );
    auto totalTimeStr = Utils::formatTimestamp(totalSeconds);

    cout << endl;
    for (size_t frameNo = 0; frameNo < totalFrames; frameNo++) {
        int readFrameNo = videoReader.readFrame(buffer, 1);
        doPadding(
                buffer, _arguments.size,
                paddedBuffer, this->_cachedPaddingSize
        );

        outputBuffer.clear();
        this->encodeJpeg(
                paddedRgbaPtr, totalPixels,
                outputBuffer,
                passData
        );

        if (_writeIntermediateResult) {
            writeBuffer(
                    _arguments.tmpDir + "/output-" + to_string(frameNo) + ".jpg",
                    outputBuffer.data(), outputBuffer.size()
            );
        }

        aviOutputStream.writeFrame(outputBuffer.data(), outputBuffer.size());

        auto currentTime = Utils::getCurrentTimestamp(frameNo + 1, videoReader.getTotalFrames(), _arguments.fps);
        auto currentTimeStr = Utils::formatTimestamp(currentTime);
        cout << "\u001B[A" << std::flush
             << "Time: " << Utils::formatTimestamp(currentTime) << " / " << totalTimeStr
             << endl;
    }
    aviOutputStream.close();

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;
    delete[] buffer;
    delete[] paddedBuffer;
}

void MJPEGEncoderOpenCLImpl::finalize() {
    _clCmdQueue->finish();
}

void MJPEGEncoderOpenCLImpl::bootstrap() {
    vector<cl::Platform> platforms;
    vector<cl::Device> devices;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        throw runtime_error("Cannot found any OpenCL platform");
    }

    cout << "Found " << platforms.size() << " platforms" << endl;

    cl::Device *firstMatchDevice = nullptr;

    cl_int deviceType = CL_DEVICE_TYPE_CPU;
    switch (this->_arguments.device) {
        case GPU:
            deviceType = CL_DEVICE_TYPE_GPU;
            break;
        case CPU:
            deviceType = CL_DEVICE_TYPE_CPU;
            break;

        default:
            deviceType = CL_DEVICE_TYPE_ALL;
            break;
    }

    string platformName;
    for (auto &p : platforms) {
        platformName = p.getInfo<CL_PLATFORM_NAME>(nullptr);
        p.getDevices(deviceType, &devices);

        if (!devices.empty()) {
            firstMatchDevice = new cl::Device(devices[0]);
            break;
        }
    }

    if (!firstMatchDevice) {
        throw runtime_error("Cannot found any OpenCL device.");
    }

    _device = firstMatchDevice;
    cout << "Using device ["
         << firstMatchDevice->getInfo<CL_DEVICE_NAME>()
         << "] on platform [" << platformName
         << "]." << endl;

    _context = new cl::Context(*_device);

    string kernelCode = readClKernelFile("jpeg-encoder.cl");

    _program = new cl::Program(*_context, kernelCode);

    auto buildRet = _program->build(devices);
    if (buildRet == CL_BUILD_PROGRAM_FAILURE) {
        auto buildInfo = _program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*_device);
        cerr << buildInfo << endl;
    }
    this->dieIfClError(buildRet, __LINE__);
}

MJPEGEncoderOpenCLImpl::~MJPEGEncoderOpenCLImpl() {
    delete _clCmdQueue;
    delete _context;
    delete _device;

    _clCmdQueue = nullptr;
    _context = nullptr;
    _device = nullptr;
}

void MJPEGEncoderOpenCLImpl::dieIfClError(cl_int err, int line) {
    if (err != CL_SUCCESS) {
        string msg = "OpenCL Error: ";
        msg += to_string(err);
        msg += " (";
        msg += getClError(err);
        msg += ")";

        if (line) {
            msg += ", line: ";
            msg += to_string(line);
        }
        throw runtime_error(msg);
    }
}

string MJPEGEncoderOpenCLImpl::readClKernelFile(const char *path) const {
    std::ifstream file;

    string searchPaths[] = {
            "../opencl-kernel/",
            "./opencl-kernel/",
            "../",
            "./",
    };
    for (string &prefix : searchPaths) {
        file.open(prefix + path, std::ios::binary | std::ios::ate);
        if (file.good())
            break;
    }

    if (!file.good()) {
        throw runtime_error("cannot open kernel file: " + string(path));
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer(size + 1, '\0');

    if (!file.read(buffer.data(), size)) {
        throw runtime_error((string("Failed to read file ") + path).c_str());
    }
    return buffer;
}

const char *MJPEGEncoderOpenCLImpl::getClError(cl_int err) {
#define CaseReturnString(x) case x: return #x;
    switch (err) {
        CaseReturnString(CL_SUCCESS)
        CaseReturnString(CL_DEVICE_NOT_FOUND)
        CaseReturnString(CL_DEVICE_NOT_AVAILABLE)
        CaseReturnString(CL_COMPILER_NOT_AVAILABLE)
        CaseReturnString(CL_MEM_OBJECT_ALLOCATION_FAILURE)
        CaseReturnString(CL_OUT_OF_RESOURCES)
        CaseReturnString(CL_OUT_OF_HOST_MEMORY)
        CaseReturnString(CL_PROFILING_INFO_NOT_AVAILABLE)
        CaseReturnString(CL_MEM_COPY_OVERLAP)
        CaseReturnString(CL_IMAGE_FORMAT_MISMATCH)
        CaseReturnString(CL_IMAGE_FORMAT_NOT_SUPPORTED)
        CaseReturnString(CL_BUILD_PROGRAM_FAILURE)
        CaseReturnString(CL_MAP_FAILURE)
        CaseReturnString(CL_MISALIGNED_SUB_BUFFER_OFFSET)
        CaseReturnString(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST)
        CaseReturnString(CL_COMPILE_PROGRAM_FAILURE)
        CaseReturnString(CL_LINKER_NOT_AVAILABLE)
        CaseReturnString(CL_LINK_PROGRAM_FAILURE)
        CaseReturnString(CL_DEVICE_PARTITION_FAILED)
        CaseReturnString(CL_KERNEL_ARG_INFO_NOT_AVAILABLE)
        CaseReturnString(CL_INVALID_VALUE)
        CaseReturnString(CL_INVALID_DEVICE_TYPE)
        CaseReturnString(CL_INVALID_PLATFORM)
        CaseReturnString(CL_INVALID_DEVICE)
        CaseReturnString(CL_INVALID_CONTEXT)
        CaseReturnString(CL_INVALID_QUEUE_PROPERTIES)
        CaseReturnString(CL_INVALID_COMMAND_QUEUE)
        CaseReturnString(CL_INVALID_HOST_PTR)
        CaseReturnString(CL_INVALID_MEM_OBJECT)
        CaseReturnString(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)
        CaseReturnString(CL_INVALID_IMAGE_SIZE)
        CaseReturnString(CL_INVALID_SAMPLER)
        CaseReturnString(CL_INVALID_BINARY)
        CaseReturnString(CL_INVALID_BUILD_OPTIONS)
        CaseReturnString(CL_INVALID_PROGRAM)
        CaseReturnString(CL_INVALID_PROGRAM_EXECUTABLE)
        CaseReturnString(CL_INVALID_KERNEL_NAME)
        CaseReturnString(CL_INVALID_KERNEL_DEFINITION)
        CaseReturnString(CL_INVALID_KERNEL)
        CaseReturnString(CL_INVALID_ARG_INDEX)
        CaseReturnString(CL_INVALID_ARG_VALUE)
        CaseReturnString(CL_INVALID_ARG_SIZE)
        CaseReturnString(CL_INVALID_KERNEL_ARGS)
        CaseReturnString(CL_INVALID_WORK_DIMENSION)
        CaseReturnString(CL_INVALID_WORK_GROUP_SIZE)
        CaseReturnString(CL_INVALID_WORK_ITEM_SIZE)
        CaseReturnString(CL_INVALID_GLOBAL_OFFSET)
        CaseReturnString(CL_INVALID_EVENT_WAIT_LIST)
        CaseReturnString(CL_INVALID_EVENT)
        CaseReturnString(CL_INVALID_OPERATION)
        CaseReturnString(CL_INVALID_GL_OBJECT)
        CaseReturnString(CL_INVALID_BUFFER_SIZE)
        CaseReturnString(CL_INVALID_MIP_LEVEL)
        CaseReturnString(CL_INVALID_GLOBAL_WORK_SIZE)
        CaseReturnString(CL_INVALID_PROPERTY)
        CaseReturnString(CL_INVALID_IMAGE_DESCRIPTOR)
        CaseReturnString(CL_INVALID_COMPILER_OPTIONS)
        CaseReturnString(CL_INVALID_LINKER_OPTIONS)
        CaseReturnString(CL_INVALID_DEVICE_PARTITION_COUNT)
    }

    return "Unknown Error";
}
