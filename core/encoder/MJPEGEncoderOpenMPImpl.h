//
// Created by wcl on 2020/11/07.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include <cassert>
#include <omp.h>
#include "../Utils.h"

namespace core::encoder {
    class MJPEGEncoderOpenMPImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderOpenMPImpl(const Arguments &arguments);

    protected:
        bool _writeIntermediateResult = false;

        void start() override;

        void finalize() override;

        void encodeJpeg(
                color::RGBA *paddedData, int length,
                vector<char> &output,
                void** sharedData
        ) override;

        int16_t encodeBlock(
                vector<char>& output,
                float block[8][8],
                const float scaled[8*8],
                int16_t lastDC,
                const BitCode huffmanDC[256],
                const BitCode huffmanAC[256], 
                const BitCode* codewords,
                BitBuffer& bitBuffer
        );

        void transformColorSpace(
                color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
                const Size &frameSize
        ) const override;

        void doPadding(
                char *__restrict originalBuffer, const Size &originalSize,
                char *__restrict targetBuffer, const Size &targetSize
        ) override;
    };
}
