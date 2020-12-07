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

    if (_writeIntermediateResult) {
        writeBuffer(
                _arguments.tmpDir + "/yuv444.raw",
                nullptr, 0
        );
    }
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
    auto totalPixels = *((size_t *) sharedData[argc++]);
    auto dimPtr = (cl::NDRange *) sharedData[argc++];

    cl_int err = 0;

    // copy rgba data to kernel
    err = this->_clCmdQueue->enqueueWriteBuffer(*dRgbaBuffer, CL_TRUE, 0, length, (char *) originalData);
    this->dieIfClError(err, __LINE__);

    err = this->_clCmdQueue->enqueueNDRangeKernel(
            *clPaddingAndTransformColorSpace,
            cl::NullRange,
            *dimPtr,
            cl::NullRange
    );

    this->dieIfClError(err, __LINE__);


    // TODO: convert to jpeg in opencl
}

void MJPEGEncoderOpenCLImpl::start() {
    this->bootstrap();

    RawVideoReader videoReader(_arguments.input, _arguments.size);
    auto totalFrames = videoReader.getTotalFrames();
    const auto totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

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

    auto buffer = new char[originalRgbFrameSize * maxBatchFrames];
    AVIOutputStream aviOutputStream(_arguments.output);

    (&aviOutputStream)
            ->setFps(_arguments.fps)
            ->setSize(_arguments.size)
            ->setTotalFrames(videoReader.getTotalFrames());

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer;

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
    auto hYChannel = new color::YCbCr444::ChannelData[batchDataSizeOneChannel];
    auto hCbChannel = new color::YCbCr444::ChannelData[batchDataSizeOneChannel];
    auto hCrChannel = new color::YCbCr444::ChannelData[batchDataSizeOneChannel];


    // init opencl buffer
    cl::Buffer dRgbaBuffer(*_context, CL_MEM_READ_ONLY, originalRgbFrameSize * maxBatchFrames);
    cl::Buffer dYChannelBuffer(*_context, CL_MEM_READ_WRITE, sizeof(color::YCbCr444::ChannelData) * batchDataSizeOneChannel);
    cl::Buffer dCbChannelBuffer(*_context, CL_MEM_READ_WRITE, sizeof(color::YCbCr444::ChannelData) * batchDataSizeOneChannel);
    cl::Buffer dCrChannelBuffer(*_context, CL_MEM_READ_WRITE, sizeof(color::YCbCr444::ChannelData) * batchDataSizeOneChannel);
    cl::Buffer dOtherArgs(
            *_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            sizeof(int) * paddingKernelArgs.size(),
            paddingKernelArgs.data()
    );

    // cl program
    cl::Kernel clPaddingAndTransformColorSpace(*_program, "paddingAndTransformColorSpace");
    unsigned int transformArgc = 0;
    clPaddingAndTransformColorSpace.setArg(transformArgc++, dRgbaBuffer);

    clPaddingAndTransformColorSpace.setArg(transformArgc++, dYChannelBuffer);
    clPaddingAndTransformColorSpace.setArg(transformArgc++, dCbChannelBuffer);
    clPaddingAndTransformColorSpace.setArg(transformArgc++, dCrChannelBuffer);
    clPaddingAndTransformColorSpace.setArg(transformArgc++, dOtherArgs);

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
            nullptr,
    };

    string yuvDataTmpPath = _arguments.tmpDir + "/yuv444.raw";
    for (size_t frameNo = 0; frameNo < totalFrames; frameNo += maxBatchFrames) {
        int readFrameCnt = videoReader.readFrame(buffer, maxBatchFrames);
        outputBuffer.clear();

        cl::NDRange globalSize(groupWidth, groupHeight,
                               readFrameCnt * (extraNeedBatchPerFrameX * extraNeedBatchPerFrameY));

        passData[6] = &globalSize;

        this->encodeJpeg(
                (color::RGBA *) buffer, originalRgbFrameSize * readFrameCnt,
                outputBuffer,
                passData
        );

        if (_writeIntermediateResult) {
            size_t dataSize = readFrameCnt * totalPixels * sizeof(color::YCbCr444::ChannelData);
            memset(hYChannel, '\0', dataSize);
            memset(hCbChannel, '\0', dataSize);
            memset(hCrChannel, '\0', dataSize);
            _clCmdQueue->enqueueReadBuffer(dYChannelBuffer, CL_TRUE,
                                           0, dataSize,
                                           hYChannel);
            _clCmdQueue->enqueueReadBuffer(dCbChannelBuffer, CL_TRUE,
                                           0, dataSize,
                                           hCbChannel);
            _clCmdQueue->enqueueReadBuffer(dCrChannelBuffer, CL_TRUE,
                                           0, dataSize,
                                           hCrChannel);

            for (auto j = 0; j < readFrameCnt; j++) {
                auto offset = totalPixels * j;
                writeBuffer(yuvDataTmpPath,
                            (char*)(hYChannel + offset), totalPixels,
                            true
                );
                writeBuffer(yuvDataTmpPath,
                            (char*)(hCbChannel + offset), totalPixels,
                            true
                );
                writeBuffer(yuvDataTmpPath,
                            (char*)(hCrChannel + offset), totalPixels,
                            true
                );
            }
        }

        aviOutputStream.writeFrame(outputBuffer.data(), outputBuffer.size());

        auto currentTime = Utils::getCurrentTimestamp(frameNo + readFrameCnt - 1, videoReader.getTotalFrames(),
                                                      _arguments.fps);
        auto currentTimeStr = Utils::formatTimestamp(currentTime);
        cout << "\u001B[A" << std::flush
             << "Time: " << Utils::formatTimestamp(currentTime) << " / " << totalTimeStr
             << endl;
    }
    aviOutputStream.close();

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;

    delete[] hYChannel;
    delete[] hCbChannel;
    delete[] hCrChannel;
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
//    _maxWorkItems[0] = 128;
//    _maxWorkItems[1] = 128;
    _maxWorkGroupSize = firstMatchDevice->getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();

    _context = new cl::Context({*_device});

    string kernelCode = readClKernelFile("jpeg-encoder.cl");

    _program = new cl::Program(*_context, kernelCode);

    auto buildRet = _program->build({*_device});
    if (buildRet == CL_BUILD_PROGRAM_FAILURE) {
        auto buildInfo = _program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*_device);
        cerr << buildInfo << endl;
    }
    this->dieIfClError(buildRet, __LINE__);

    _clCmdQueue = new cl::CommandQueue(*_context, *_device);
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
