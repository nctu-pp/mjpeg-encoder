//
// Created by wcl on 2020/11/05.
//

#pragma once

#include "AbstractMJPEGEncoder.h"
#include "../../io/RawVideoReader.h"

namespace core::encoder {
    class MJPEGEncoderSerialImpl : public AbstractMJPEGEncoder {
    public:
        explicit MJPEGEncoderSerialImpl(const Arguments &arguments);

    protected:
        void start() override;

        void finalize() override;
    };
}
