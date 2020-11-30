//
// Created by wcl on 2020/11/05.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include "../Utils.h"
#include <cassert>
#include <CL/cl.hpp>
#include <sstream>

namespace core::encoder {
    class MJPEGEncoderOpenCLImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderOpenCLImpl(const Arguments &arguments);
        ~MJPEGEncoderOpenCLImpl();

    protected:
        bool _writeIntermediateResult = true;

        void start() override;

        void finalize() override;

        void encodeJpeg(
                color::RGBA *originalData, int length,
                vector<char> &output,
                void **sharedData
        ) override;

    private:
        void bootstrap();
        cl::Device* _device;
        cl::Context * _context;
        cl::Program* _program;
        cl::CommandQueue* _clCmdQueue;
        cl_int _maxWorkGroupSize;
        vector<cl_ulong> _maxWorkItems;

        void dieIfClError(cl_int err, int line = 0);

        string readClKernelFile(const char* path) const;

        static const char* getClError(cl_int err);
    };
}
