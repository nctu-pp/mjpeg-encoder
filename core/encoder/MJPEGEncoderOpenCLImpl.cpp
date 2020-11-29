//
// Created by wcl on 2020/11/05.
//

#include "MJPEGEncoderOpenCLImpl.h"

using namespace io;
using namespace std;
using namespace core;
using namespace core::encoder;


MJPEGEncoderOpenCLImpl::MJPEGEncoderOpenCLImpl(const Arguments &arguments)
        : AbstractMJPEGEncoder(arguments) {
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

void MJPEGEncoderOpenCLImpl::encodeJpeg(
        color::RGBA *paddedData, int length,
        vector<char> &output,
        void **sharedData
) {
    auto yuvFrameBuffer = static_cast<color::YCbCr444 *>(sharedData[0]);
    transformColorSpace(paddedData, *yuvFrameBuffer, this->_cachedPaddingSize);

    _bitBuffer.init();
    writeJFIFHeader(output);

    writeQuantizationTable(output, _quantLuminance, _quantChrominance);

    // write infos: SOF0 - start of frame
    writeImageInfos(output);

    writeHuffmanTable(output);

    writeScanInfo(output);

    // precompute JPEG codewords for quantized DCT
    BitCode codewordsArray[
            2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
    BitCode *codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
    uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
    int32_t mask = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
    for (int16_t value = 1; value < CodeWordLimit; value++) {
        // numBits = position of highest set bit (ignoring the sign)
        // mask    = (2^numBits) - 1
        if (value > mask) // one more bit ?
        {
            numBits++;
            mask = (mask << 1) | 1; // append a set bit
        }
        codewords[-value] = BitCode(mask - value,
                                    numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
        codewords[+value] = BitCode(value, numBits);
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
                auto row = (mcuY + deltaY > maxHeight) ? maxHeight : mcuY + deltaY;
                for (auto deltaX = 0; deltaX < 8; ++deltaX) {
                    auto pixelPos = row * (maxWidth + 1) + column;
                    column = (column < maxWidth) ? column + 1 : column;

                    Y[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getYChannel()[pixelPos]) - 128;
                    Cb[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCbChannel()[pixelPos]) - 128;
                    Cr[deltaY][deltaX] = static_cast<float>(yuvFrameBuffer->getCrChannel()[pixelPos]) - 128;

                }
            }
            lastYDC = encodeBlock(output, Y, _scaledLuminance, lastYDC, _huffmanLuminanceDC, _huffmanLuminanceAC,
                                  codewords);
            lastCbDC = encodeBlock(output, Cb, _scaledChrominance, lastCbDC, _huffmanChrominanceDC,
                                   _huffmanChrominanceAC, codewords);
            lastCrDC = encodeBlock(output, Cr, _scaledChrominance, lastCrDC, _huffmanChrominanceDC,
                                   _huffmanChrominanceAC, codewords);

        } // end mcuX
    } // end mcuY

    writeBitCode(output, BitCode(0x7F, 7), _bitBuffer);

    output.push_back(0xFF);
    output.push_back(0xD9);
}

void MJPEGEncoderOpenCLImpl::start() {
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

    void *passData[] = {
            &yuvFrameBuffer,
            nullptr,
    };

    auto totalSeconds = Utils::getCurrentTimestamp(
            videoReader.getTotalFrames(), videoReader.getTotalFrames(),
            _arguments.fps
    );
    auto totalTimeStr = Utils::formatTimestamp(totalSeconds);

    cout << endl;
    for (size_t frameNo = 0; frameNo < totalFrames; frameNo++) {
        int readFrameNo = videoReader.readFrame(buffer, 1);
        doPadding(
                buffer, _arguments.size,
                paddedBuffer, this->_cachedPaddingSize
        );

        outputBuffer.clear();
        this->encodeJpeg(
                paddedRgbaPtr, totalPixels,
                outputBuffer,
                passData
        );

        if (_writeIntermediateResult) {
            writeBuffer(
                    _arguments.tmpDir + "/output-" + to_string(frameNo) + ".jpg",
                    outputBuffer.data(), outputBuffer.size()
            );
        }

        aviOutputStream.writeFrame(outputBuffer.data(), outputBuffer.size());

        auto currentTime = Utils::getCurrentTimestamp(frameNo + 1, videoReader.getTotalFrames(), _arguments.fps);
        auto currentTimeStr = Utils::formatTimestamp(currentTime);
        cout << "\u001B[A" << std::flush
             << "Time: " << Utils::formatTimestamp(currentTime) << " / " << totalTimeStr
             << endl;
    }
    aviOutputStream.close();

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;
    delete[] buffer;
    delete[] paddedBuffer;
}

void MJPEGEncoderOpenCLImpl::finalize() {

}
