//
// Created by wcl on 2020/11/05.
//

#pragma once

#include <memory>
#include <memory.h>
#include <fstream>
#include <vector>

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
                color::RGBA *paddedData, int length, int quality,
                vector<char>& output
        ) const = 0;

        void calcPaddingSize();

        virtual void transformColorSpace(
                color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
                const Size &frameSize
        ) const;

        Size getPaddingSize() const;

        Arguments _arguments;
        Size _cachedPaddingSize;

        static void doPadding(
                char *__restrict originalBuffer, const Size &originalSize,
                char *__restrict targetBuffer, const Size &targetSize
        );

        static void writeBuffer(const string &path, char *buffer, size_t len, bool append = false);

        const static size_t BLOCK_SIZE = 8;
    };
}

