//
// Created by wcl on 2020/11/05.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include "../Utils.h"
#include <cassert>

namespace core::encoder {
    class MJPEGEncoderSerialImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderSerialImpl(const Arguments &arguments);

    protected:
        bool _writeIntermediateResult = true;
        ofstream _yuvTmpData;

        void start() override;

        void finalize() override;

        void encodeJpeg(
                color::RGBA *paddedData, int length, int quality,
                vector<char> &output,
                void** sharedData
        ) override;
    };
}
