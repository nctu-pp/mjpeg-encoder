//
// Created by wcl on 2020/11/05.
//

#pragma once

#include <memory>

#include "../../model/Model.h"
#include "../../io/RawVideoReader.h"
#include "../AVIOutputStream.h"

using namespace std;
using namespace model;

namespace core::encoder {

    class AbstractMJPEGEncoder {
    public:
        virtual void start() = 0;

        virtual void finalize() = 0;

        static shared_ptr<AbstractMJPEGEncoder> getInstance(const Arguments &arguments);

    protected:
        explicit AbstractMJPEGEncoder(const Arguments &arguments);

        virtual void encodeJpeg(
                color::RGBA *data, int length, int quality,
                char *outBuffer, int &outLength
        ) const;

        Arguments _arguments;
    };
}

