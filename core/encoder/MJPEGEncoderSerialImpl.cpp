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
    auto maxWidth = (this->_cachedPaddingSize).width;
    auto maxHeight = (this->_cachedPaddingSize).height;
    int number_of_blocks = (maxHeight/8)*(maxWidth/8);
    int index = 0, offset;

    const auto blockAlign = 8 * 8 * sizeof(float);

    auto yBlock = static_cast<JpegBlockType*>(__builtin_assume_aligned(sharedData[1], blockAlign));
    auto cbBlock = static_cast<JpegBlockType*>(__builtin_assume_aligned(sharedData[2], blockAlign));
    auto crBlock = static_cast<JpegBlockType*>(__builtin_assume_aligned(sharedData[3], blockAlign));

    for (auto mcuY = 0; mcuY < maxHeight; mcuY += 8) { // each step is either 8 or 16 (=mcuSize)
        for (auto mcuX = 0; mcuX < maxWidth; mcuX += 8) {
            offset = mcuY*maxWidth+mcuX;
            index = mcuY/8 * (maxWidth/8) + mcuX/8;

            memcpy(yBlock[index][0], yuvFrameBuffer->getYChannel()+maxWidth*0+offset, sizeof(float)*8);
            memcpy(yBlock[index][1], yuvFrameBuffer->getYChannel()+maxWidth*1+offset, sizeof(float)*8);
            memcpy(yBlock[index][2], yuvFrameBuffer->getYChannel()+maxWidth*2+offset, sizeof(float)*8);
            memcpy(yBlock[index][3], yuvFrameBuffer->getYChannel()+maxWidth*3+offset, sizeof(float)*8);
            memcpy(yBlock[index][4], yuvFrameBuffer->getYChannel()+maxWidth*4+offset, sizeof(float)*8);
            memcpy(yBlock[index][5], yuvFrameBuffer->getYChannel()+maxWidth*5+offset, sizeof(float)*8);
            memcpy(yBlock[index][6], yuvFrameBuffer->getYChannel()+maxWidth*6+offset, sizeof(float)*8);
            memcpy(yBlock[index][7], yuvFrameBuffer->getYChannel()+maxWidth*7+offset, sizeof(float)*8);
            
            memcpy(cbBlock[index][0], yuvFrameBuffer->getCbChannel()+maxWidth*0+offset, sizeof(float)*8);
            memcpy(cbBlock[index][1], yuvFrameBuffer->getCbChannel()+maxWidth*1+offset, sizeof(float)*8);
            memcpy(cbBlock[index][2], yuvFrameBuffer->getCbChannel()+maxWidth*2+offset, sizeof(float)*8);
            memcpy(cbBlock[index][3], yuvFrameBuffer->getCbChannel()+maxWidth*3+offset, sizeof(float)*8);
            memcpy(cbBlock[index][4], yuvFrameBuffer->getCbChannel()+maxWidth*4+offset, sizeof(float)*8);
            memcpy(cbBlock[index][5], yuvFrameBuffer->getCbChannel()+maxWidth*5+offset, sizeof(float)*8);
            memcpy(cbBlock[index][6], yuvFrameBuffer->getCbChannel()+maxWidth*6+offset, sizeof(float)*8);
            memcpy(cbBlock[index][7], yuvFrameBuffer->getCbChannel()+maxWidth*7+offset, sizeof(float)*8);
			
            memcpy(crBlock[index][0], yuvFrameBuffer->getCrChannel()+maxWidth*0+offset, sizeof(float)*8);
            memcpy(crBlock[index][1], yuvFrameBuffer->getCrChannel()+maxWidth*1+offset, sizeof(float)*8);
            memcpy(crBlock[index][2], yuvFrameBuffer->getCrChannel()+maxWidth*2+offset, sizeof(float)*8);
            memcpy(crBlock[index][3], yuvFrameBuffer->getCrChannel()+maxWidth*3+offset, sizeof(float)*8);
            memcpy(crBlock[index][4], yuvFrameBuffer->getCrChannel()+maxWidth*4+offset, sizeof(float)*8);
            memcpy(crBlock[index][5], yuvFrameBuffer->getCrChannel()+maxWidth*5+offset, sizeof(float)*8);
            memcpy(crBlock[index][6], yuvFrameBuffer->getCrChannel()+maxWidth*6+offset, sizeof(float)*8);
            memcpy(crBlock[index][7], yuvFrameBuffer->getCrChannel()+maxWidth*7+offset, sizeof(float)*8);
            
            doDCT(yBlock[index], _scaledLuminance);
            doDCT(cbBlock[index], _scaledChrominance);
            doDCT(crBlock[index], _scaledChrominance);
        } // end mcuX
    } // end mcuY

    for(int i = 0; i < number_of_blocks; ++i) {
        lastYDC = encodeBlock(output, yBlock[i], lastYDC, _huffmanLuminanceDC, _huffmanLuminanceAC, codewords);
        lastCbDC = encodeBlock(output, cbBlock[i], lastCbDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords);
        lastCrDC = encodeBlock(output, crBlock[i], lastCrDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords);
    }

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
    auto numberOfBlocks = yuvFrameBuffer.getPerChannelSize() >> 6; // (w*h) / 64
    auto yBlock = new float[numberOfBlocks][8][8];
    auto cbBlock = new float[numberOfBlocks][8][8];
    auto crBlock = new float[numberOfBlocks][8][8];

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer;
    outputBuffer.reserve(2 * 1024 * 1024); // 2MB

    aviOutputStream.start();

    void *passData[] = {
            &yuvFrameBuffer,
            yBlock,
            cbBlock,
            crBlock,
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
    delete[] yBlock;
    delete[] cbBlock;
    delete[] crBlock;
}

void MJPEGEncoderSerialImpl::finalize() {

}
