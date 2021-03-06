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
        bool _writeIntermediateResult = false;

        void start() override;

        void finalize() override;
    };
}
