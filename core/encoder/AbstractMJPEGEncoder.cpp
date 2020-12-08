//
// Created by wcl on 2020/11/05.
//

#include "AbstractMJPEGEncoder.h"
#include "MJPEGEncoderSerialImpl.h"
#include "MJPEGEncoderOpenMPImpl.h"
#include "MJPEGEncoderOpenCLImpl.h"

using namespace core::encoder;

core::encoder::AbstractMJPEGEncoder::AbstractMJPEGEncoder(const Arguments &arguments) {
    this->_arguments = arguments;
    calcPaddingSize();
    initJpegTable();
}

shared_ptr<AbstractMJPEGEncoder> core::encoder::AbstractMJPEGEncoder::getInstance(
        const Arguments &arguments
) {
    switch (arguments.kind) {
        case model::OpenMP:
            return shared_ptr<AbstractMJPEGEncoder>(new MJPEGEncoderOpenMPImpl(arguments));

        case model::OpenCL:
            return shared_ptr<AbstractMJPEGEncoder>(new MJPEGEncoderOpenCLImpl(arguments));

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

void AbstractMJPEGEncoder::DCT(
        float *block,
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

void AbstractMJPEGEncoder::doDCT(float block[8][8], const float scaled[8*8]) const{
    // "linearize" the 8x8 block, treat it as a flat array of 64 floats
    auto block64 = (float*) block;

    // DCT: rows
    for (auto offset = 0; offset < 8; offset++)
        DCT(block64 + offset*8, 1);
    // DCT: columns
    for (auto offset = 0; offset < 8; offset++)
        DCT(block64 + offset*1, 8);

    // scale
    for (auto i = 0; i < 8*8; i++)
        block64[i] *= scaled[i];
}

int16_t AbstractMJPEGEncoder::encodeBlock(vector<char>& output, float block[8][8], int16_t lastDC,
                    const BitCode huffmanDC[256], const BitCode huffmanAC[256], const BitCode* codewords)
{
    auto block64 = (float*) block;
// cout << "ok0:" << block64[0] << endl;
    // encode DC (the first coefficient is the "average color" of the 8x8 block)
    auto DC = int(block64[0] + (block64[0] >= 0 ? +0.5f : -0.5f)); // C++11's nearbyint() achieves a similar effect

    // quantize and zigzag the other 63 coefficients
    auto posNonZero = 0; // find last coefficient which is not zero (because trailing zeros are encoded differently)
    int16_t quantized[8*8];
    for (auto i = 1; i < 8*8; i++) // start at 1 because block64[0]=DC was already processed
    {
        auto value = block64[ZigZagInv[i]];
        // round to nearest integer
        quantized[i] = int(value + (value >= 0 ? +0.5f : -0.5f)); // C++11's nearbyint() achieves a similar effect
        // remember offset of last non-zero coefficient
        if (quantized[i] != 0)
            posNonZero = i;
    }
// cout << "ok1:" << DC << endl;
    // same "average color" as previous block ?
    auto diff = DC - lastDC;
    if (diff == 0)
        writeBitCode(output, huffmanDC[0x00], _bitBuffer);
    else
    {
        auto& bits = codewords[diff]; // nope, encode the difference to previous block's average color
        writeBitCode(output, huffmanDC[bits.numBits], _bitBuffer);
        writeBitCode(output, bits, _bitBuffer);
    }
// cout << "ok2" << endl;
    // encode ACs (quantized[1..63])
    auto offset = 0; // upper 4 bits count the number of consecutive zeros
    for (auto i = 1; i <= posNonZero; i++) // quantized[0] was already written, skip all trailing zeros, too
    {
        // zeros are encoded in a special way
        while (quantized[i] == 0) // found another zero ?
        {
            offset    += 0x10; // add 1 to the upper 4 bits
            // split into blocks of at most 16 consecutive zeros
            if (offset > 0xF0) // remember, the counter is in the upper 4 bits, 0xF = 15
            {
                writeBitCode(output, huffmanAC[0xF0], _bitBuffer);
                offset = 0;
            }
            i++;
        }

        auto& encoded = codewords[quantized[i]];
        // combine number of zeros with the number of bits of the next non-zero value
        writeBitCode(output, huffmanAC[offset + encoded.numBits], _bitBuffer);
        writeBitCode(output, encoded, _bitBuffer);
        offset = 0;
    }

    // send end-of-block code (0x00), only needed if there are trailing zeros
    if (posNonZero < 8*8 - 1) // = 63
        writeBitCode(output, huffmanAC[0x00], _bitBuffer);
    return DC;
}

void AbstractMJPEGEncoder::addMarker(
        vector<char>& output,
        uint8_t id,
        uint16_t length
) {
    output.insert(output.end(), {(char)0xFF, (char)id, (char)(length >> 8), (char)(length & 0xFF)});
}

void AbstractMJPEGEncoder::writeBitCode(
        vector<char>& output,
        const BitCode& data,
        BitBuffer& bitBuffer
) {
    bitBuffer.numBits += data.numBits;
    bitBuffer.data   <<= data.numBits;
    bitBuffer.data    |= data.code;

    uint8_t localBuffer[bitBuffer.numBits >> 2]; // / 4
    size_t localBufferLen = 0;
    while(bitBuffer.numBits >= 8) {
        bitBuffer.numBits -= 8;
        auto oneByte = uint8_t(bitBuffer.data >> bitBuffer.numBits);
        localBuffer[localBufferLen++] = oneByte;
        if (oneByte == 0xFF)
            localBuffer[localBufferLen++] = 0;
    }

    output.insert(output.end(), localBuffer, localBuffer + localBufferLen);
};

void AbstractMJPEGEncoder::writeJFIFHeader(
        vector<char>& output
) const {
    output.insert(output.end(), { (char)0xFF, (char)0xD8,         // SOI marker (start of image)
            (char)0xFF, (char)0xE0,         // JFIF APP0 tag
            0,16,              // length: 16 bytes (14 bytes payload + 2 bytes for this length field)
            'J','F','I','F',0, // JFIF identifier, zero-terminated
            1,1,               // JFIF version 1.1
            0,                 // no density units specified
            0,1,0,1,           // density: 1 pixel "per pixel" horizontally and vertically
            0,0 });             // no thumbnail (size 0 x 0));
}

void AbstractMJPEGEncoder::writeQuantizationTable(
        vector<char>& output,
        const uint8_t quantLuminance[64],
        const uint8_t quantChrominance[64]
) {
    addMarker(output, 0xDB, 2+2*(1+8*8));
    output.emplace_back(0x00);
    output.insert(output.end(), &quantLuminance[0], &quantLuminance[64]);
    output.emplace_back(0x01);
    output.insert(output.end(), &quantChrominance[0], &quantChrominance[64]);
}

void AbstractMJPEGEncoder::writeHuffmanTable(
        vector<char>& output
) {
    addMarker(output, 0xC4, 2+208+208);
    output.emplace_back(0x00);
    for (auto i = 0; i < 16; ++i) output.emplace_back(DcLuminanceCodesPerBitsize[i]);
    for (auto i = 0; i < 12; ++i) output.emplace_back(DcLuminanceValues[i]);
    output.emplace_back(0x10);
    for (auto i = 0; i < 16; ++i) output.emplace_back(AcLuminanceCodesPerBitsize[i]);
    for (auto i = 0; i < 162; ++i) output.emplace_back(AcLuminanceValues[i]);
    output.emplace_back(0x01);
    for (auto i = 0; i < 16; ++i) output.emplace_back(DcChrominanceCodesPerBitsize[i]);
    for (auto i = 0; i < 12; ++i) output.emplace_back(DcChrominanceValues[i]);
    output.emplace_back(0x11);
    for (auto i = 0; i < 16; ++i) output.emplace_back(AcChrominanceCodesPerBitsize[i]);
    for (auto i = 0; i < 162; ++i) output.emplace_back(AcChrominanceValues[i]);
}

void AbstractMJPEGEncoder::writeImageInfos(
        vector<char>& output
) {
    addMarker(output, 0xC0, 2+6+3*3);
    output.emplace_back(0x08);

    output.emplace_back(this->_arguments.size.height >> 8);
    output.emplace_back(this->_arguments.size.height & 0xFF);
    output.emplace_back(this->_arguments.size.width >> 8);
    output.emplace_back(this->_arguments.size.width & 0xFF);
    output.emplace_back(3);
    for (auto id = 1; id <= 3; ++id) {
        output.emplace_back(id);
        output.emplace_back(0x11);
        output.emplace_back((id ==  1) ? 0 : 1);
    }
}

void AbstractMJPEGEncoder::writeScanInfo(
        vector<char>& output
) {
    // start of scan
    addMarker(output, 0xDA, 2+1+2*3+3);
    output.emplace_back(3);
    for (auto id = 1; id <= 3; ++id) {
        output.emplace_back(id);
        output.emplace_back((id == 1) ? 0x00 : 0x11);
    }
    static const uint8_t Spectral[3] = { 0, 63, 0 }; // spectral selection: must be from 0 to 63; successive approximation must be 0
    for (auto i = 0; i < 3; ++i) output.emplace_back(Spectral[i]);
}

void AbstractMJPEGEncoder::initJpegTable() {
    // generate huffmanLuminanceDC and huffmanLuminanceAC first
    generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, _huffmanLuminanceDC);
    generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, _huffmanLuminanceAC);
    // generate huffmanChrominanceDC and huffmanChrominanceAC first
    generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, _huffmanChrominanceDC);
    generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, _huffmanChrominanceAC);
    int quality = _arguments.quality;

    quality = clamp(quality, 1, 100);
    quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

    for (auto i = 0; i < 8 * 8; ++i) {
        int luminance = (DefaultQuantLuminance[ZigZagInv[i]] * quality + 50) / 100;
        int chrominance = (DefaultQuantChrominance[ZigZagInv[i]] * quality + 50) / 100;

        // clamp to 1..255
        _quantLuminance[i] = clamp(luminance, 1, 255);
        _quantChrominance[i] = clamp(chrominance, 1, 255);
    }

    for (auto i = 0; i < 8 * 8; ++i) {
        auto row = ZigZagInv[i] / 8; // same as ZigZagInv[i] >> 3
        auto column = ZigZagInv[i] % 8; // same as ZigZagInv[i] &  7
        auto factor = 1 / (AanScaleFactors[row] * AanScaleFactors[column] * 8);
        _scaledLuminance[ZigZagInv[i]] = factor / _quantLuminance[i];
        _scaledChrominance[ZigZagInv[i]] = factor / _quantChrominance[i];
    }
}

void AbstractMJPEGEncoder::transformColorSpace(
        color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
        const Size &srcSize, const Size &dstSize) const {
    const auto totalRows = dstSize.height;
    const auto totalCols = dstSize.width;

    auto *__restrict yChannelBase = yuv444Buffer.getYChannel();
    auto *__restrict cbChannelBase = yuv444Buffer.getCbChannel();
    auto *__restrict crChannelBase = yuv444Buffer.getCrChannel();

    for (auto row = 0; row < totalRows; row++) {
        auto offset = (totalCols * row);

        for (auto col = 0; col < totalCols; col++) {
            auto srcCol = col < srcSize.width ? col : srcSize.width - 1;
            auto srcRow = row < srcSize.height ? row : srcSize.height - 1;
            color::RGBA *__restrict ptr = rgbaBuffer + srcRow * srcSize.width + srcCol;

            const auto r = (float) ((ptr->value >> 16) & 0xFF);
            const auto g = (float) ((ptr->value >> 8) & 0xFF);
            const auto b = (float) ((ptr->value >> 0) & 0xFF);

            auto yF = (+0.299f * r + 0.587f * g + 0.114f * b) -128.f;
            auto cbF = (-0.16874f * r - 0.33126f * g + 0.5f * b);
            auto crF = (+0.5f * r - 0.41869f * g - 0.08131f * b);

            // assert(-128 <= yF && yF <= 127);
            // assert(-128 <= cbF && cbF <= 127);
            // assert(-128 <= crF && crF <= 127);

            yChannelBase[col + offset] = (yF);
            cbChannelBase[col + offset] = (cbF);
            crChannelBase[col + offset] = (crF);
        }
    }
}
