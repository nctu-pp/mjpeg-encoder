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
}

void MJPEGEncoderSerialImpl::encodeJpeg(
        color::RGBA *originalData, int length,
        vector<char> &output,
        void **sharedData
) {
    auto yuvFrameBuffer = static_cast<color::YCbCr444 *>(sharedData[0]);
    transformColorSpace(originalData, *yuvFrameBuffer, this->_arguments.size, this->_cachedPaddingSize);

    _bitBuffer.init();
    writeJFIFHeader(output);

    writeQuantizationTable(output, _quantLuminance, _quantChrominance);

    // write infos: SOF0 - start of frame
    writeImageInfos(output);

    writeHuffmanTable(output);

    writeScanInfo(output);

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
                auto row = (mcuY + deltaY > maxHeight) ? maxHeight : mcuY + deltaY;
                for (auto deltaX = 0; deltaX < 8; ++deltaX) {
                    auto pixelPos = row * (maxWidth+1) + column;
                    column = (column < maxWidth) ? column + 1: column;

                    Y[deltaY][deltaX] = yuvFrameBuffer->getYChannel()[pixelPos];
                    Cb[deltaY][deltaX] = yuvFrameBuffer->getCbChannel()[pixelPos];
                    Cr[deltaY][deltaX] = yuvFrameBuffer->getCrChannel()[pixelPos];

                }
            }
            lastYDC = encodeBlock(output, Y, _scaledLuminance, lastYDC, _huffmanLuminanceDC, _huffmanLuminanceAC, codewords);
            lastCbDC = encodeBlock(output, Cb, _scaledChrominance, lastCbDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords);
            lastCrDC = encodeBlock(output, Cr, _scaledChrominance, lastCrDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords);

        } // end mcuX
    } // end mcuY

    writeBitCode(output, BitCode(0x7F, 7), _bitBuffer);

    output.push_back(0xFF);
    output.push_back(0xD9);
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
        outputBuffer.clear();
        this->encodeJpeg(
                (color::RGBA*)buffer, totalPixels,
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
        /*
        Utils::printProgress(
                cout,
                string("Time: ") +
                Utils::formatTimestamp(currentTime) +
                " / " +
                totalTimeStr
        );
         */
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

void MJPEGEncoderSerialImpl::finalize() {

}
