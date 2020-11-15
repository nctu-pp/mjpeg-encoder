//
// Created by wcl on 2020/11/07.
//

#include "MJPEGEncoderOpenMPImpl.h"

using namespace io;
using namespace std;
using namespace core;
using namespace core::encoder;

MJPEGEncoderOpenMPImpl::MJPEGEncoderOpenMPImpl(const Arguments &arguments) : AbstractMJPEGEncoder(
        arguments) {
    if (arguments.numThreads > 0)
        omp_set_num_threads(arguments.numThreads);
}

void MJPEGEncoderOpenMPImpl::start() {
    const auto maxThreads = omp_get_max_threads();

    /**
     * TODO: In addition to do anything in serial, also need declare some local buffer, local output buffer for
     *       sub-function do encode in theirs buffer by thread-id, and pass them by sharedData parameters when
     *       using sharedData.
     *       One important things is: do not re-create stl container (e.g., vector), or destructor may cost many
     *       time. Reuse them if possible.
     */
}

void MJPEGEncoderOpenMPImpl::finalize() {

}

void MJPEGEncoderOpenMPImpl::encodeJpeg(color::RGBA *paddedData, int length, int quality, vector<char> &output,
                                        void **sharedData) {

}

void MJPEGEncoderOpenMPImpl::transformColorSpace(
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
#pragma omp parallel for simd default(none) \
            shared( \
                totalRows, totalCols, \
                b, g, r, \
                rgbaBuffer, yChannelBase, cbChannelBase, crChannelBase \
            )
    for (auto row = 0; row < totalRows; row++) {
        auto offset = (totalCols * row);
        auto rgbaColorPtr = rgbaBuffer + offset;
        auto *__restrict yChannel = yChannelBase + offset;
        auto *__restrict cbChannel = cbChannelBase + offset;
        auto *__restrict crChannel = crChannelBase + offset;

#pragma omp ordered simd
        for (auto col = 0; col < totalCols; col++) {
            auto colorPtr = rgbaColorPtr + col;
            b[col] = colorPtr->color.b;
            g[col] = colorPtr->color.g;
            r[col] = colorPtr->color.r;
        }

#pragma omp ordered simd
        for (auto col = 0; col < totalCols; col++) {
            auto yF = (int) (+0.299f * r[col] + 0.587f * g[col] + 0.114f * b[col]);
            auto cbF = (int) (-0.16874f * r[col] - 0.33126f * g[col] + 0.5f * b[col] + 128.f);
            auto crF = (int) (+0.5f * r[col] - 0.41869f * g[col] - 0.08131f * b[col] + 128.f);

            yChannel[col] = (char) (yF & 0xff);
            cbChannel[col] = (char) (cbF & 0xff);
            crChannel[col] = (char) (crF & 0xff);
        }
    }
}

void MJPEGEncoderOpenMPImpl::doPadding(char *originalBuffer, const Size &originalSize, char *targetBuffer,
                                       const Size &targetSize) {
    const auto originalRowStep = originalSize.width * io::RawVideoReader::PIXEL_BYTES;
    const auto targetRowStep = targetSize.width * io::RawVideoReader::PIXEL_BYTES;
    const auto requireColPaddingCnt = targetSize.width - originalSize.width;
    const auto requireRowPaddingCnt = targetSize.height - originalSize.height;
    const auto origTotalRows = originalSize.height;
    const auto origTotalCols = originalSize.width;
    const auto padTotalRows = targetSize.height;
    const auto padTotalCols = targetSize.width;

#pragma omp parallel for simd default(none) \
        shared( \
            origTotalRows, \
            originalRowStep, targetRowStep, \
            originalBuffer, targetBuffer \
        )
    for (auto row = 0; row < origTotalRows; row++) {
        char *__restrict originalPtr = originalBuffer + (row * originalRowStep);
        char *__restrict targetPtr = targetBuffer + (row * targetRowStep);
        memcpy(targetPtr, originalPtr, originalRowStep); // aaa
    }


    // do column padding
    if (requireColPaddingCnt != 0) {
#pragma omp parallel for simd default(none) \
        shared( \
            targetBuffer, origTotalRows, \
            origTotalCols, padTotalCols, \
            requireColPaddingCnt \
        )
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

#pragma omp parallel for simd default(none) \
        shared( \
            targetBuffer, refPixelPtr, \
            origTotalRows, padTotalRows, \
            padTotalCols, targetRowStep \
        )
        for (auto row = origTotalRows; row < padTotalRows; row++) {
            char *__restrict targetPixelPtr = targetBuffer + io::RawVideoReader::PIXEL_BYTES * (row * padTotalCols);
            memcpy(targetPixelPtr, refPixelPtr, targetRowStep);
        }
    }
}
