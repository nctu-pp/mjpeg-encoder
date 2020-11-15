//
// Created by wcl on 2020/11/07.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include <cassert>
#include <omp.h>

namespace core::encoder {
    class MJPEGEncoderOpenMPImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderOpenMPImpl(const Arguments &arguments);

    protected:

        void start() override;

        void finalize() override;

        void encodeJpeg(
                color::RGBA *paddedData, int length, int quality,
                vector<char> &output,
                void** sharedData
        ) override;

        void transformColorSpace(
                color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
                const Size &frameSize
        ) const override;
    };
}
