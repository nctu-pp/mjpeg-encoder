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
    this->_maxWorkGroupSize = 0;
    this->_maxMemoryAllocSize = 0;
    this->_maxGlobalMemorySize = 0;

    // if (_writeIntermediateResult) {
    //     writeBuffer(
    //             _arguments.tmpDir + "/yuv444.raw",
    //             nullptr, 0
    //     );
    // }
}

void MJPEGEncoderOpenCLImpl::encodeJpeg(
        color::RGBA *originalData, int length,
        vector<char> &output,
        void **sharedData
) {
    unsigned int argc = 0;
    auto dRgbaBuffer = (cl::Buffer *) sharedData[argc++];

    auto dYChannelBuffer = (cl::Buffer *) sharedData[argc++];
    auto dCbChannelBuffer = (cl::Buffer *) sharedData[argc++];
    auto dCrChannelBuffer = (cl::Buffer *) sharedData[argc++];

    auto clPaddingAndTransformColorSpace = (cl::Kernel *) sharedData[argc++];
    auto totalPixels = ((size_t *) sharedData[argc++]);
    auto dimPtr = (cl::NDRange *) sharedData[argc++];

    auto clDoDCT = (cl::Kernel *) sharedData[argc++];
    auto blockDimPtr = (cl::NDRange *) sharedData[argc++];

    auto dOtherArgs = (cl::Buffer *) sharedData[argc++];

    auto batchDataSizeOneChannel = (unsigned long *) sharedData[argc++];

    auto readFrameCnt = (int *) sharedData[argc++];
    auto scaledLuminance = (cl::Buffer *) sharedData[argc++];
    auto scaledChrominance = (cl::Buffer *) sharedData[argc++];
    auto outputSize = (vector<int> *) sharedData[argc++];

    auto clEncode = (cl::Kernel *) sharedData[argc++];

    auto huffmanLuminanceAC = (cl::Buffer *) sharedData[argc++];
    auto huffmanChrominanceAC = (cl::Buffer *) sharedData[argc++];
    auto huffmanLuminanceDC = (cl::Buffer *) sharedData[argc++];
    auto huffmanChrominanceDC = (cl::Buffer *) sharedData[argc++];
    auto zigzaginv = (cl::Buffer *) sharedData[argc++];
    auto codewordsBuffer = (cl::Buffer *) sharedData[argc++];

    auto outputBuffer = (cl::Buffer *) sharedData[argc++];
    auto outputLength = (cl::Buffer *) sharedData[argc++];
    auto hOutputBuffer = (char*) sharedData[argc++];
    auto dOutputBufferSize = *((size_t*) sharedData[argc++]);
    auto perFrameOutputBufferSize = *((size_t*) sharedData[argc++]);

    cl_int err = 0;
    cl::Event transformEvent, yDctEvent, cbDctEvent, crDctEvent, encodeEvent;

    TEST_TIME_START(gpu);

    // copy rgba data to kernel
    err = this->_clCmdQueue->enqueueWriteBuffer(*dRgbaBuffer, CL_TRUE, 0, length, (char *) originalData);
    this->dieIfClError(err, __LINE__);

    TEST_TIME_START(run1);
    err = this->_clCmdQueue->enqueueNDRangeKernel(
            *clPaddingAndTransformColorSpace,
            cl::NullRange,
            *dimPtr,
            cl::NullRange,
            nullptr,
            &transformEvent
    );
    this->dieIfClError(err, __LINE__);

    this->dieIfClError(transformEvent.wait(), __LINE__);
    cout << "PaddingAndTransformColorSpace Time " << TEST_TIME_END(run1) << endl;

    auto doDctAndQuantization = [
            &blockDimPtr,
            &clDoDCT, &dOtherArgs,
            &err, this
    ](
            cl::Buffer *inputBuffer, cl::Buffer *outputBuffer, cl::Buffer *scaled,
            const vector<cl::Event> &waitingEvents,
            cl::Event *targetEvent
    ) {
        TEST_TIME_START(dct1);

        err = clDoDCT->setArg(0, *dOtherArgs);
        err = clDoDCT->setArg(1, *inputBuffer);
        err = clDoDCT->setArg(2, *outputBuffer);

        clDoDCT->setArg(3, *scaled);
        this->dieIfClError(err, __LINE__);

        err = this->_clCmdQueue->enqueueNDRangeKernel(
                *clDoDCT,
                cl::NullRange,
                *blockDimPtr,
                cl::NullRange,
                &waitingEvents,
                targetEvent
        );
        this->dieIfClError(err, __LINE__);

        // cout << "dct " << TEST_TIME_END(dct1) << endl;
    };

    TEST_TIME_START(run2);
    // do dct and quantization for y channel
    doDctAndQuantization(dYChannelBuffer, dYChannelBuffer, scaledLuminance,
                         {transformEvent}, &yDctEvent);

    // do dct and quantization for cb channel
    doDctAndQuantization(dCbChannelBuffer, dCbChannelBuffer, scaledChrominance,
                         {transformEvent}, &cbDctEvent);

    // do dct and quantization for cr channel
    doDctAndQuantization(dCrChannelBuffer, dCrChannelBuffer, scaledChrominance,
                         {transformEvent}, &crDctEvent);


    vector<cl::Event> dctAndQuantizationEvents(
            {
                    yDctEvent, cbDctEvent, crDctEvent
            });
    this->dieIfClError(cl::Event::waitForEvents(dctAndQuantizationEvents), __LINE__);
    cout << "dctAndQuantization Time " << TEST_TIME_END(run2) << endl;

    auto maxWidth = (this->_cachedPaddingSize).width;
    auto maxHeight = (this->_cachedPaddingSize).height;
    int current = 0;

    TEST_TIME_START(run3);
    err = this->_clCmdQueue->enqueueNDRangeKernel(
            *clEncode,
            cl::NullRange,
            cl::NDRange((*readFrameCnt)),
            cl::NullRange,
            &dctAndQuantizationEvents,
            &encodeEvent
    );
    this->dieIfClError(err, __LINE__);

    this->dieIfClError(encodeEvent.wait(), __LINE__);
    cout << "clEncode Time " << TEST_TIME_END(run3) << endl;

    cout << "GPU TIME: " << TEST_TIME_END(gpu) << endl;

    TEST_TIME_START(readTime);
    int outputLengLocal[*readFrameCnt];
    int headerSize = commonJpegHeader.size();
    err = _clCmdQueue->enqueueReadBuffer(*outputLength, CL_TRUE,
                                         0, sizeof(int) * (*readFrameCnt),
                                         outputLengLocal);
    this->dieIfClError(err, __LINE__);

    err = _clCmdQueue->enqueueReadBuffer(*outputBuffer, CL_TRUE,
                                         0, dOutputBufferSize,
                                         hOutputBuffer);
    this->dieIfClError(err, __LINE__);
    cout << "DATA READ BACK TIME: " << TEST_TIME_END(readTime) << endl;

    TEST_TIME_START(cpu);

    for (auto j = 0; j < (*readFrameCnt); j++) {
        if (!outputLengLocal[j]) continue;

        char *outputBufferLocal = hOutputBuffer + j * perFrameOutputBufferSize;

        output.insert(output.end(), commonJpegHeader.begin(), commonJpegHeader.end());
        output.insert(output.end(), outputBufferLocal,
                         outputBufferLocal + outputLengLocal[j]);
        outputSize->push_back(outputLengLocal[j]+headerSize);
    }

    cout << "CPU TIME: " << TEST_TIME_END(cpu) << endl;

    cout << endl;
}

void MJPEGEncoderOpenCLImpl::start() {
    TEST_TIME_START(devInitTime);
    this->bootstrap();
    cout << "Dev Init Time: " << TEST_TIME_END(devInitTime) << endl;

    RawVideoReader videoReader(_arguments.input, _arguments.size);
    auto totalFrames = videoReader.getTotalFrames();
    const size_t totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

    size_t originalRgbFrameSize = videoReader.getPerFrameSize();

    auto maxBatchFrames = this->_maxWorkItems[2];
    auto extraNeedBatchPerFrameX = 1;
    auto extraNeedBatchPerFrameY = 1;
    do {
        extraNeedBatchPerFrameX = ((_cachedPaddingSize.width / this->_maxWorkItems[0]) + 1);
        extraNeedBatchPerFrameY = ((_cachedPaddingSize.height / this->_maxWorkItems[1]) + 1);

        maxBatchFrames /= (extraNeedBatchPerFrameX * extraNeedBatchPerFrameY);

        assert(this->_maxWorkItems[0] == this->_maxWorkItems[1]);
    } while (false);

    auto maxFrameAllowedReadInMemory = this->_maxGlobalMemorySize / (
            totalPixels * (
                    sizeof(color::RGBA) +
                    sizeof(float) * 3 * 2
            ) + 64*2+256 * sizeof(BitCodeStruct) * 4 + 64
              + sizeof(BitCodeStruct) * 2 * CodeWordLimit
              + sizeof(char) * totalPixels * maxBatchFrames
              + sizeof(int) * maxBatchFrames
    );

    maxBatchFrames = std::min(maxFrameAllowedReadInMemory, maxBatchFrames);

    auto buffer = new char[originalRgbFrameSize * maxBatchFrames];
    AVIOutputStream aviOutputStream(_arguments.output);

    (&aviOutputStream)
            ->setFps(_arguments.fps)
            ->setSize(_arguments.size)
            ->setTotalFrames(videoReader.getTotalFrames());

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer;
    vector<int> outputSize;

    aviOutputStream.start();

    auto totalSeconds = Utils::getCurrentTimestamp(
            videoReader.getTotalFrames(), videoReader.getTotalFrames(),
            _arguments.fps
    );
    auto totalTimeStr = Utils::formatTimestamp(totalSeconds);

    cout << endl;

    // init kernel function parameters
    vector<int> paddingKernelArgs;
    do {
        paddingKernelArgs.push_back(_arguments.size.width);
        paddingKernelArgs.push_back(_arguments.size.height);

        paddingKernelArgs.push_back(_cachedPaddingSize.width);
        paddingKernelArgs.push_back(_cachedPaddingSize.height);

        paddingKernelArgs.push_back(_maxWorkItems[0]);
        paddingKernelArgs.push_back(_maxWorkItems[2]);

        paddingKernelArgs.push_back(extraNeedBatchPerFrameX);
        paddingKernelArgs.push_back(extraNeedBatchPerFrameY);
    } while (false);

    auto batchDataSizeOneChannel = totalPixels * maxBatchFrames;

    cl_int bufferDeclErr = CL_SUCCESS;

    // init opencl buffer
    cl::Buffer dRgbaBuffer(*_context, CL_MEM_READ_ONLY,
                           originalRgbFrameSize * maxBatchFrames, nullptr, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    auto yCbCrSize = sizeof(color::YCbCr444::ChannelData) * batchDataSizeOneChannel;
    cl::Buffer dYChannelBuffer(*_context, CL_MEM_READ_WRITE, yCbCrSize, nullptr, &bufferDeclErr);
    cl::Buffer dCbChannelBuffer(*_context, CL_MEM_READ_WRITE, yCbCrSize, nullptr, &bufferDeclErr);
    cl::Buffer dCrChannelBuffer(*_context, CL_MEM_READ_WRITE, yCbCrSize, nullptr, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer scaledLuminance(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                               sizeof(float) * 64, _scaledLuminance, &bufferDeclErr);
    cl::Buffer scaledChrominance(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(float) * 64, _scaledChrominance, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer dOtherArgs(
            *_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            sizeof(int) * paddingKernelArgs.size(),
            paddingKernelArgs.data()
    );

    BitCodeStruct tmp_LAC[256], tmp_CAC[256], tmp_LDC[256], tmp_CDC[256];

    for (int i = 0; i < 256; ++i) {
        tmp_LAC[i].code = _huffmanLuminanceAC[i].code;
        tmp_LAC[i].numBits = _huffmanLuminanceAC[i].numBits;
        tmp_CAC[i].code = _huffmanChrominanceAC[i].code;
        tmp_CAC[i].numBits = _huffmanChrominanceAC[i].numBits;
        tmp_LDC[i].code = _huffmanLuminanceDC[i].code;
        tmp_LDC[i].numBits = _huffmanLuminanceDC[i].numBits;
        tmp_CDC[i].code = _huffmanChrominanceDC[i].code;
        tmp_CDC[i].numBits = _huffmanChrominanceDC[i].numBits;
    }

    cl::Buffer huffmanLuminanceAC(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(BitCodeStruct) * 256, tmp_LAC, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer huffmanChrominanceAC(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(BitCodeStruct) * 256, tmp_CAC, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer huffmanLuminanceDC(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(BitCodeStruct) * 256, tmp_LDC, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer huffmanChrominanceDC(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(BitCodeStruct) * 256, tmp_CDC, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer zigzaginv(*_context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(uint8_t) * 64, ZigZagInv, &bufferDeclErr);
    this->dieIfClError(bufferDeclErr, __LINE__);

    cl::Buffer codewordsBuffer(*_context, CL_MEM_READ_ONLY,
                               sizeof(BitCodeStruct) * 2 * CodeWordLimit, nullptr, &bufferDeclErr);

    // precompute JPEG codewords for quantized DCT
    do {
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

        // copy BitCode to BitCodeStruct and shift -2048~2048 to 0~4096
        BitCodeStruct codewords_tmp[2 * CodeWordLimit];
        for (int i = 0; i < 2 * CodeWordLimit - 1; ++i) {
            codewords_tmp[i].code = codewords[i - CodeWordLimit + 1].code;
            codewords_tmp[i].numBits = codewords[i - CodeWordLimit + 1].numBits;
        }
        this->dieIfClError(
                this->_clCmdQueue->enqueueWriteBuffer(codewordsBuffer, CL_TRUE, 0,
                                                      sizeof(BitCodeStruct) * 2 * CodeWordLimit, codewords_tmp),
                __LINE__
        );
    } while(false);

    // HACK: assume frame never bigger than original size
    size_t perFrameOutputBufferSize = totalPixels * 3;
    size_t dOutputBufferSize = maxBatchFrames * perFrameOutputBufferSize;
    cl::Buffer dOutputLength(*_context, CL_MEM_WRITE_ONLY, sizeof(int) * maxBatchFrames, nullptr, &bufferDeclErr);
    char *hOutBuffer = new char[dOutputBufferSize];
    cl::Buffer dOutputBuffer(*_context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
                             dOutputBufferSize, hOutBuffer, &bufferDeclErr);

    // cl program
    cl::Kernel clPaddingAndTransformColorSpace(*_program, "paddingAndTransformColorSpace");
    cl::Kernel clDoDCT(*_program, "doDCT");
    cl::Kernel clDoEncode(*_program, "encode");

    do {
        unsigned int transformArgc = 0;
        clPaddingAndTransformColorSpace.setArg(transformArgc++, dRgbaBuffer);
        clPaddingAndTransformColorSpace.setArg(transformArgc++, dYChannelBuffer);
        clPaddingAndTransformColorSpace.setArg(transformArgc++, dCbChannelBuffer);
        clPaddingAndTransformColorSpace.setArg(transformArgc++, dCrChannelBuffer);
        clPaddingAndTransformColorSpace.setArg(transformArgc++, dOtherArgs);
    } while(false);

    do {
        clDoEncode.setArg(0, dYChannelBuffer);
        clDoEncode.setArg(1, dCbChannelBuffer);
        clDoEncode.setArg(2, dCrChannelBuffer);
        clDoEncode.setArg(3, huffmanLuminanceAC);
        clDoEncode.setArg(4, huffmanChrominanceAC);
        clDoEncode.setArg(5, huffmanLuminanceDC);
        clDoEncode.setArg(6, huffmanChrominanceDC);
        clDoEncode.setArg(7, zigzaginv);
        clDoEncode.setArg(8, dOutputBuffer);
        clDoEncode.setArg(9, dOutputLength);
        clDoEncode.setArg(10, dOtherArgs);
        clDoEncode.setArg(11, codewordsBuffer);
        clDoEncode.setArg(12, (cl_int) perFrameOutputBufferSize);
    } while(false);

    this->dieIfClError(
            this->_clCmdQueue->enqueueWriteBuffer(scaledLuminance, CL_TRUE, 0, sizeof(float) * 64, _scaledLuminance),
            __LINE__
    );
    this->dieIfClError(
            this->_clCmdQueue->enqueueWriteBuffer(scaledChrominance, CL_TRUE, 0, sizeof(float) * 64,
                                                  _scaledChrominance),
            __LINE__
    );

    auto copiedTotalPixels = totalPixels;
    auto groupWidth = std::min((cl_ulong) _cachedPaddingSize.width, _maxWorkItems[0]);
    auto groupHeight = std::min((cl_ulong) _cachedPaddingSize.height, _maxWorkItems[1]);

    void *passData[] = {
            &dRgbaBuffer,

            &dYChannelBuffer,
            &dCbChannelBuffer,
            &dCrChannelBuffer,

            &clPaddingAndTransformColorSpace,

            &copiedTotalPixels,
            nullptr, // 6
            &clDoDCT,
            nullptr, // 8
            &dOtherArgs,

            &batchDataSizeOneChannel,
            nullptr, // 11
            &scaledLuminance,
            &scaledChrominance,
            &outputSize,

            &clDoEncode,
            &huffmanLuminanceAC,
            &huffmanChrominanceAC,
            &huffmanLuminanceDC,
            &huffmanChrominanceDC,
            &zigzaginv,
            &codewordsBuffer,

            &dOutputBuffer,
            &dOutputLength,
            hOutBuffer,
            &dOutputBufferSize,
            &perFrameOutputBufferSize,
    };

//     string yuvDataTmpPath = _arguments.tmpDir + "/yuv444.raw";
    for (size_t frameNo = 0; frameNo < totalFrames; frameNo += maxBatchFrames) {
        int readFrameCnt = videoReader.readFrame(buffer, maxBatchFrames);
        outputBuffer.clear();
        outputSize.clear();

        cl::NDRange globalSize(groupWidth, groupHeight,
                               readFrameCnt * (extraNeedBatchPerFrameX * extraNeedBatchPerFrameY));

        passData[6] = &globalSize;
        // cout << groupWidth << " " << groupHeight << " " << readFrameCnt << " " << extraNeedBatchPerFrameX << " " << extraNeedBatchPerFrameY << endl;

        cl::NDRange globalBlockSize(_cachedPaddingSize.width / 8, _cachedPaddingSize.height / 8,
                                    readFrameCnt);
        passData[8] = &globalBlockSize;

        passData[11] = &readFrameCnt;

        this->encodeJpeg(
                (color::RGBA *) buffer, originalRgbFrameSize * readFrameCnt,
                outputBuffer,
                passData
        );

        auto dataPtr = outputBuffer.data();
        for (auto j = 0; j < readFrameCnt; ++j) {
            aviOutputStream.writeFrame(dataPtr, outputSize[j]);
            dataPtr += outputSize[j];
        }

        auto currentTime = Utils::getCurrentTimestamp(frameNo + readFrameCnt - 1, videoReader.getTotalFrames(),
                                                      _arguments.fps);
        auto currentTimeStr = Utils::formatTimestamp(currentTime);

        cout << "\u001B[A" << std::flush
             << "Time: " << Utils::formatTimestamp(currentTime) << " / " << totalTimeStr
             << endl;
    }
    cout << "\u001B[A" << std::flush
         << "Time: " << totalTimeStr << " / " << totalTimeStr
         << endl;
    aviOutputStream.close();

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;

    delete[] hOutBuffer;
    delete[] buffer;
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

    _maxWorkItems = firstMatchDevice->getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
    _maxWorkGroupSize = firstMatchDevice->getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    _maxMemoryAllocSize = firstMatchDevice->getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    _maxGlobalMemorySize = firstMatchDevice->getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();

    _context = new cl::Context({*_device});

    string kernelCode = readClKernelFile("jpeg-encoder.cl");

    _program = new cl::Program(*_context, kernelCode);

#ifdef __WIN32__
    const char* buildOpts = "-cl-mad-enable -cl-unsafe-math-optimizations -cl-fast-relaxed-math -cl-strict-aliasing -D__WIN32__";
#else
    const char* buildOpts = "-cl-mad-enable -cl-unsafe-math-optimizations -cl-fast-relaxed-math -cl-strict-aliasing";
#endif

    auto buildRet = _program->build({*_device}, buildOpts);
    if (buildRet == CL_BUILD_PROGRAM_FAILURE) {
        auto buildInfo = _program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*_device);
        cerr << buildInfo << endl;
    }
    this->dieIfClError(buildRet, __LINE__);

    _clCmdQueue = new cl::CommandQueue(*_context, *_device);

    // test image size
    if (std::max(_cachedPaddingSize.width, _cachedPaddingSize.height) / 8 > _maxWorkItems[0]) {
        throw runtime_error("Image too large, not support yet.");
    }
}

MJPEGEncoderOpenCLImpl::~MJPEGEncoderOpenCLImpl() {
    delete _clCmdQueue;
    delete _program;
    delete _context;
    delete _device;

    _clCmdQueue = nullptr;
    _program = nullptr;
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
