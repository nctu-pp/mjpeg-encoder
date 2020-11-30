//
// Created by wcl on 2020/11/05.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include "../Utils.h"
#include <cassert>
#include <CL/cl.hpp>

namespace core::encoder {
    class MJPEGEncoderOpenCLImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderOpenCLImpl(const Arguments &arguments);
        ~MJPEGEncoderOpenCLImpl();

    protected:
        bool _writeIntermediateResult = false;

        void start() override;

        void finalize() override;

        void encodeJpeg(
                color::RGBA *paddedData, int length,
                vector<char> &output,
                void **sharedData
        ) override;

    private:
        void bootstrap();
        cl::Device* _device;
        cl::Context * _context;
        cl::CommandQueue* _clCmdQueue;

        void dieIfClError(cl_int err, int line = 0);
    };
}
