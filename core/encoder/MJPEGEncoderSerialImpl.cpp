//
// Created by wcl on 2020/11/05.
//

#include "MJPEGEncoderSerialImpl.h"

using namespace io;
using namespace std;
using namespace core;
using namespace core::encoder;


MJPEGEncoderSerialImpl::MJPEGEncoderSerialImpl(const Arguments &arguments)
        : AbstractMJPEGEncoder(arguments) {
    if (_writeIntermediateResult) {
        _yuvTmpData.open(_arguments.tmpDir + "/yuv444.raw",
                         std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    }
}

void MJPEGEncoderSerialImpl::encodeJpeg(
        color::RGBA *paddedData, int length,
        int quality,
        vector<char> &output,
        void **sharedData
) {
    // TODO: move _yuvFrameBuffer to shared data
    auto yuvFrameBuffer = static_cast<color::YCbCr444 *>(sharedData[0]);
    transformColorSpace(paddedData, *yuvFrameBuffer, this->_cachedPaddingSize);

    // set JPEG Header
    const uint8_t HeaderJfif[2+2+16] =
        { 0xFF,0xD8,         // SOI marker (start of image)
            0xFF,0xE0,         // JFIF APP0 tag
            0,16,              // length: 16 bytes (14 bytes payload + 2 bytes for this length field)
            'J','F','I','F',0, // JFIF identifier, zero-terminated
            1,1,               // JFIF version 1.1
            0,                 // no density units specified
            0,1,0,1,           // density: 1 pixel "per pixel" horizontally and vertically
            0,0 };             // no thumbnail (size 0 x 0)
    for (auto i = 0; i < 2+2+16; ++i)
        output.push_back(HeaderJfif[i]);

    quality = clamp(quality, 1, 100);
    quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

    uint8_t quantLuminance  [8*8];
    uint8_t quantChrominance[8*8];

    for (auto i = 0; i < 8*8; ++i)
    {
        int luminance   = (DefaultQuantLuminance  [ZigZagInv[i]] * quality + 50) / 100;
        int chrominance = (DefaultQuantChrominance[ZigZagInv[i]] * quality + 50) / 100;

        // clamp to 1..255
        quantLuminance  [i] = clamp(luminance,   1, 255);
        quantChrominance[i] = clamp(chrominance, 1, 255);
    }

    // write quantization tables
    addMarker(output, 0xDB, 2+2*(1+8*8));
    output.push_back(0x00);
    for (auto i = 0; i < 8*8; ++i) output.push_back(quantLuminance[i]);
    output.push_back(0x01);
    for (auto i = 0; i < 8*8; ++i) output.push_back(quantChrominance[i]);

    // write info
    addMarker(output, 0xC0, 2+6+3*3);
    output.push_back(0x08);
    output.push_back((this->_cachedPaddingSize.height >> 8) << (this->_cachedPaddingSize.height & 0xFF));
    output.push_back((this->_cachedPaddingSize.width >> 8) << (this->_cachedPaddingSize.width & 0xFF));
    output.push_back(3);
    for (auto id = 1; id <= 3; ++id) {
        output.push_back(id);
        output.push_back(0x11);
        output.push_back((id ==  1) ? 0 : 1);
    }

    // Huffman table
    addMarker(output, 0xC4, 2+208+208);
    output.push_back(0x00);
    for (auto i = 0; i < 16; ++i) output.push_back(DcLuminanceCodesPerBitsize[i]);
    for (auto i = 0; i < 12; ++i) output.push_back(DcLuminanceValues[i]);
    output.push_back(0x01);
    for (auto i = 0; i < 16; ++i) output.push_back(AcLuminanceCodesPerBitsize[i]);
    for (auto i = 0; i < 162; ++i) output.push_back(AcLuminanceValues[i]);    

    BitCode huffmanLuminanceDC[256];
    BitCode huffmanLuminanceAC[256];
    generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, huffmanLuminanceDC);
    generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, huffmanLuminanceAC);

    BitCode huffmanChrominanceDC[256];
    BitCode huffmanChrominanceAC[256];

    output.push_back(0x01);
    for (auto i = 0; i < 16; ++i) output.push_back(DcChrominanceCodesPerBitsize[i]);
    for (auto i = 0; i < 12; ++i) output.push_back(DcChrominanceValues[i]);
    output.push_back(0x11);
    for (auto i = 0; i < 16; ++i) output.push_back(AcChrominanceCodesPerBitsize[i]);
    for (auto i = 0; i < 162; ++i) output.push_back(AcChrominanceValues[i]);    

    generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, huffmanChrominanceDC);
    generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, huffmanChrominanceAC);

    // start of scan
    addMarker(output, 0xDA, 2+1+2*3+3);
    output.push_back(3);
    for (auto id = 1; id <= 3; ++id) {
        output.push_back(id);
        output.push_back((id == 1) ? 0x00 : 0x11);
    }
    static const uint8_t Spectral[3] = { 0, 63, 0 }; // spectral selection: must be from 0 to 63; successive approximation must be 0
    for (auto i = 0; i < 3; ++i) output.push_back(Spectral[i]);


    float scaledLuminance  [8*8];
    float scaledChrominance[8*8];

    for (auto i = 0; i < 8*8; ++i)
    {
        auto row    = ZigZagInv[i] / 8; // same as ZigZagInv[i] >> 3
        auto column = ZigZagInv[i] % 8; // same as ZigZagInv[i] &  7

        // scaling constants for AAN DCT algorithm: AanScaleFactors[0] = 1, AanScaleFactors[k=1..7] = cos(k*PI/16) * sqrt(2)
        static const float AanScaleFactors[8] = { 1, 1.387039845f, 1.306562965f, 1.175875602f, 1, 0.785694958f, 0.541196100f, 0.275899379f };
        auto factor = 1 / (AanScaleFactors[row] * AanScaleFactors[column] * 8);
        scaledLuminance  [ZigZagInv[i]] = factor / quantLuminance  [i];
        scaledChrominance[ZigZagInv[i]] = factor / quantChrominance[i];
    }

    // precompute JPEG codewords for quantized DCT
    BitCode  codewordsArray[2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
    BitCode* codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
    uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
    int32_t mask    = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
    for (int16_t value = 1; value < CodeWordLimit; value++)
    {
        // numBits = position of highest set bit (ignoring the sign)
        // mask    = (2^numBits) - 1
        if (value > mask) // one more bit ?
        {
        numBits++;
        mask = (mask << 1) | 1; // append a set bit
        }
        codewords[-value] = BitCode(mask - value, numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
        codewords[+value] = BitCode(       value, numBits);
    }

    // average color of the previous MCU
    int16_t lastYDC = 0, lastCbDC = 0, lastCrDC = 0;
    auto maxWidth = (this->_cachedPaddingSize).width - 1;
    auto maxHeight = (this->_cachedPaddingSize).height - 1;
    float Y[8][8], Cb[8][8], Cr[8][8];
    for (auto mcuY = 0; mcuY < (this->_cachedPaddingSize).height; mcuY += 8) { // each step is either 8 or 16 (=mcuSize)
        for (auto mcuX = 0; mcuX < (this->_cachedPaddingSize).width; mcuX += 8) {
            for (auto deltaY = 0; deltaY < 8; ++deltaY) {
                auto column = mcuX;
                auto row = mcuY + deltaY;
                for (auto deltaX = 0; deltaX < 8; ++deltaX) {
                    auto pixelPos = row * (maxWidth+1) + column;
                    column = (column < maxWidth) ? column + 1: column;

                    Y[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getYChannel()[pixelPos]);
                    Cb[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCbChannel()[pixelPos]);
                    Cr[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCrChannel()[pixelPos]);

                }
            }
            lastYDC = encodeBlock(output, Y, scaledLuminance, lastYDC, huffmanLuminanceDC, huffmanLuminanceAC, codewords);
            lastCbDC = encodeBlock(output, Cb, scaledChrominance, lastCbDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
            lastCrDC = encodeBlock(output, Cr, scaledChrominance, lastCrDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);

        } // end mcuX
    } // end mcuY

    output.push_back(0xFF);
    output.push_back(0xD9);

    // just for see intermediate result
    if (_writeIntermediateResult) {
        _yuvTmpData.write((char *) yuvFrameBuffer->getYChannel(), length);
        _yuvTmpData.write((char *) yuvFrameBuffer->getCbChannel(), length);
        _yuvTmpData.write((char *) yuvFrameBuffer->getCrChannel(), length);
    }
}

void MJPEGEncoderSerialImpl::start() {
    RawVideoReader videoReader(_arguments.input, _arguments.size);
    auto totalFrames = videoReader.getTotalFrames();
    const auto totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

    size_t paddedRgbFrameSize = totalPixels * RawVideoReader::PIXEL_BYTES;

    auto buffer = new char[videoReader.getPerFrameSize()];
    auto paddedBuffer = new char[paddedRgbFrameSize];
    auto paddedRgbaPtr = (color::RGBA *) paddedBuffer;
    AVIOutputStream aviOutputStream(_arguments.output);

    (&aviOutputStream)
            ->setFps(_arguments.fps)
            ->setSize(_arguments.size)
            ->setTotalFrames(videoReader.getTotalFrames());

    color::YCbCr444 yuvFrameBuffer(this->_cachedPaddingSize);

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer;

    aviOutputStream.start();

    // HACK: create empty file
    if(_writeIntermediateResult) {
        writeBuffer(_arguments.tmpDir + "/yuv444.raw", nullptr, 0);
    }

    void *passData[] = {
            &yuvFrameBuffer,
            nullptr,
    };

    for (size_t frameNo = 0; frameNo < totalFrames; frameNo++) {
        int readFrameNo = videoReader.readFrame(buffer, 1);
        doPadding(
                buffer, _arguments.size,
                paddedBuffer, this->_cachedPaddingSize
        );

        // if (_writeIntermediateResult) {
        //     writeBuffer(_arguments.tmpDir + "/pad.raw", paddedBuffer, paddedRgbFrameSize);
        // }

        outputBuffer.clear();
        this->encodeJpeg(
                paddedRgbaPtr, totalPixels,
                _arguments.quality,
                outputBuffer,
                passData
        );

        // TODO:
        /*
        if (_writeIntermediateResult) {
            writeBuffer(
                    _arguments.tmpDir + "/output-" + to_string(frameNo) + ".jpg",
                    outputBuffer.data(), outputBuffer.size()
            );
        }
        */

        aviOutputStream.writeFrame(outputBuffer.data(), outputBuffer.size());
    }
    aviOutputStream.close();

    delete[] buffer;
    delete[] paddedBuffer;
}

void MJPEGEncoderSerialImpl::finalize() {
    if (_writeIntermediateResult) {
        _yuvTmpData.close();
    }
}
