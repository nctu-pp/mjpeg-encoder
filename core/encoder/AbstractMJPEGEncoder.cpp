//
// Created by wcl on 2020/11/05.
//

#include "AbstractMJPEGEncoder.h"
#include "MJPEGEncoderSerialImpl.h"
// #include "MJPEGEncoderOpenMPImpl.h"

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
            // return shared_ptr<AbstractMJPEGEncoder>(new MJPEGEncoderOpenMPImpl(arguments));

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

void AbstractMJPEGEncoder::DCT(
        float block[8*8],
        uint8_t stride
) const {
    const auto SqrtHalfSqrt = 1.306562965f; //    sqrt((2 + sqrt(2)) / 2) = cos(pi * 1 / 8) * sqrt(2)
    const auto InvSqrt      = 0.707106781f; // 1 / sqrt(2)                = cos(pi * 2 / 8)
    const auto HalfSqrtSqrt = 0.382683432f; //     sqrt(2 - sqrt(2)) / 2  = cos(pi * 3 / 8)
    const auto InvSqrtSqrt  = 0.541196100f; // 1 / sqrt(2 - sqrt(2))      = cos(pi * 3 / 8) * sqrt(2)

    // modify in-place
    auto& block0 = block[0         ];
    auto& block1 = block[1 * stride];
    auto& block2 = block[2 * stride];
    auto& block3 = block[3 * stride];
    auto& block4 = block[4 * stride];
    auto& block5 = block[5 * stride];
    auto& block6 = block[6 * stride];
    auto& block7 = block[7 * stride];

    // based on https://dev.w3.org/Amaya/libjpeg/jfdctflt.c , the original variable names can be found in my comments
    auto add07 = block0 + block7; auto sub07 = block0 - block7; // tmp0, tmp7
    auto add16 = block1 + block6; auto sub16 = block1 - block6; // tmp1, tmp6
    auto add25 = block2 + block5; auto sub25 = block2 - block5; // tmp2, tmp5
    auto add34 = block3 + block4; auto sub34 = block3 - block4; // tmp3, tmp4

    auto add0347 = add07 + add34; auto sub07_34 = add07 - add34; // tmp10, tmp13 ("even part" / "phase 2")
    auto add1256 = add16 + add25; auto sub16_25 = add16 - add25; // tmp11, tmp12

    block0 = add0347 + add1256; block4 = add0347 - add1256; // "phase 3"

    auto z1 = (sub16_25 + sub07_34) * InvSqrt; // all temporary z-variables kept their original names
    block2 = sub07_34 + z1; block6 = sub07_34 - z1; // "phase 5"

    auto sub23_45 = sub25 + sub34; // tmp10 ("odd part" / "phase 2")
    auto sub12_56 = sub16 + sub25; // tmp11
    auto sub01_67 = sub16 + sub07; // tmp12

    auto z5 = (sub23_45 - sub01_67) * HalfSqrtSqrt;
    auto z2 = sub23_45 * InvSqrtSqrt  + z5;
    auto z3 = sub12_56 * InvSqrt;
    auto z4 = sub01_67 * SqrtHalfSqrt + z5;
    auto z6 = sub07 + z3; // z11 ("phase 5")
    auto z7 = sub07 - z3; // z13
    block1 = z6 + z4; block7 = z6 - z4; // "phase 6"
    block5 = z7 + z2; block3 = z7 - z2;
}

void AbstractMJPEGEncoder::generateHuffmanTable(
        const uint8_t numCodes[16],
        const uint8_t* values,
        BitCode result[256]
) const {
  // process all bitsizes 1 thru 16, no JPEG Huffman code is allowed to exceed 16 bits
  auto huffmanCode = 0;
  for (auto numBits = 1; numBits <= 16; numBits++)
  {
    // ... and each code of these bitsizes
    for (auto i = 0; i < numCodes[numBits - 1]; i++) // note: numCodes array starts at zero, but smallest bitsize is 1
      result[*values++] = BitCode(huffmanCode++, numBits);

    // next Huffman code needs to be one bit wider
    huffmanCode <<= 1;
  }
}
