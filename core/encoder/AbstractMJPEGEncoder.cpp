//
// Created by wcl on 2020/11/05.
//

#include "AbstractMJPEGEncoder.h"
#include "MJPEGEncoderSerialImpl.h"

using namespace core::encoder;

core::encoder::AbstractMJPEGEncoder::AbstractMJPEGEncoder(const Arguments &arguments) {
    this->_arguments = arguments;
    calcPaddingSize();
}

shared_ptr<AbstractMJPEGEncoder> core::encoder::AbstractMJPEGEncoder::getInstance(
        const Arguments &arguments
) {
    switch (arguments.kind) {
        case model::OpenMP:
        case model::OpenCL:
            return nullptr;

        case model::Serial:
        default:
            return shared_ptr<AbstractMJPEGEncoder>(new MJPEGEncoderSerialImpl(arguments));
    }
}

void AbstractMJPEGEncoder::calcPaddingSize() {
    _cachedPaddingSize = Size(_arguments.size);
    Size &s = _cachedPaddingSize;

    auto hMod = s.height % BLOCK_SIZE;
    auto wMod = s.width % BLOCK_SIZE;

    if (wMod > 0)
        s.width += BLOCK_SIZE - wMod;

    if (hMod > 0)
        s.height += BLOCK_SIZE - hMod;

    assert(_arguments.size.height <= _cachedPaddingSize.height);
    assert(_arguments.size.width <= _cachedPaddingSize.width);
}

Size AbstractMJPEGEncoder::getPaddingSize() const {
    return _cachedPaddingSize;
}

void AbstractMJPEGEncoder::doPadding(
        char *__restrict originalBuffer, const Size &originalSize,
        char *__restrict targetBuffer, const Size &targetSize
) {
    const auto originalRowStep = originalSize.width * io::RawVideoReader::PIXEL_BYTES;
    const auto targetRowStep = targetSize.width * io::RawVideoReader::PIXEL_BYTES;
    const auto requireColPaddingCnt = targetSize.width - originalSize.width;
    const auto requireRowPaddingCnt = targetSize.height - originalSize.height;
    const auto origTotalRows = originalSize.height;
    const auto origTotalCols = originalSize.width;
    const auto padTotalRows = targetSize.height;
    const auto padTotalCols = targetSize.width;

    for (auto row = 0; row < origTotalRows; row++) {
        char *__restrict originalPtr = originalBuffer + (row * originalRowStep);
        char *__restrict targetPtr = targetBuffer + (row * targetRowStep);
        memcpy(targetPtr, originalPtr, originalRowStep); // aaa
    }


    // do column padding
    if (requireColPaddingCnt != 0) {
        for (auto row = 0; row < origTotalRows; row++) {
            char *__restrict refPixelPtr = targetBuffer + io::RawVideoReader::PIXEL_BYTES *
                                                          (row * padTotalCols + (origTotalCols - 1));

            char *__restrict targetPixelPtr = refPixelPtr + io::RawVideoReader::PIXEL_BYTES;

            for (size_t i = 0, k = requireColPaddingCnt; i < k; i++) {
                memcpy(
                        targetPixelPtr + i * io::RawVideoReader::PIXEL_BYTES,
                        refPixelPtr,
                        io::RawVideoReader::PIXEL_BYTES
                );
            }
        }
    }

    // do row padding
    if (requireRowPaddingCnt != 0) {
        auto refPixelPtr = targetBuffer +
                           io::RawVideoReader::PIXEL_BYTES * ((origTotalRows - 1) * padTotalCols);

        for (auto row = origTotalRows; row < padTotalRows; row++) {

            char *__restrict targetPixelPtr = targetBuffer + io::RawVideoReader::PIXEL_BYTES * (row * padTotalCols);
            memcpy(targetPixelPtr, refPixelPtr, targetRowStep);
        }
    }
}

void AbstractMJPEGEncoder::writeBuffer(const string &path, char *buffer, size_t len, bool append) {
    ofstream fs;
    auto flag = std::ofstream::out | std::ofstream::binary;
    if (append) {
        flag |= std::ofstream::app;
    } else {
        flag |= std::ofstream::trunc;
    }
    fs.open(path, flag);

    fs.write(buffer, len);
    fs.close();
}

void AbstractMJPEGEncoder::transformColorSpace(
        color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
        const Size &frameSize
) const {
    const auto totalRows = frameSize.height;
    const auto totalCols = frameSize.width;

    float b[totalCols];
    float g[totalCols];
    float r[totalCols];
    auto *__restrict yChannelBase = yuv444Buffer.getYChannel();
    auto *__restrict cbChannelBase = yuv444Buffer.getCbChannel();
    auto *__restrict crChannelBase = yuv444Buffer.getCrChannel();

    for (auto row = 0; row < totalRows; row++) {
        auto offset = (totalCols * row);
        auto rgbaColorPtr = rgbaBuffer + offset;
        auto *__restrict yChannel = yChannelBase + offset;
        auto *__restrict cbChannel = cbChannelBase + offset;
        auto *__restrict crChannel = crChannelBase + offset;

        for (auto col = 0; col < totalCols; col++) {
            auto colorPtr = rgbaColorPtr + col;
            b[col] = colorPtr->color.b;
            g[col] = colorPtr->color.g;
            r[col] = colorPtr->color.r;
        }

        for (auto col = 0; col < totalCols; col++) {
            auto yF = (int) (+0.299f * r[col] + 0.587f * g[col] + 0.114f * b[col]);
            auto cbF = (int) (-0.16874f * r[col] - 0.33126f * g[col] + 0.5f * b[col] + 128.f);
            auto crF = (int) (+0.5f * r[col] - 0.41869f * g[col] - 0.08131f * b[col] + 128.f);

            // assert(-128 <= yF && yF <= 127);
            // assert(-128 <= cbF && cbF <= 127);
            // assert(-128 <= crF && crF <= 127);

            yChannel[col] = (char) (yF & 0xff);
            cbChannel[col] = (char) (cbF & 0xff);
            crChannel[col] = (char) (crF & 0xff);
        }
    }
}