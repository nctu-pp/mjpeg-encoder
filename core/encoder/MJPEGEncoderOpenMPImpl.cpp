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

    RawVideoReader videoReader(_arguments.input, _arguments.size);

    const auto totalFrames = videoReader.getTotalFrames();
    const auto frameSize = videoReader.getPerFrameSize();
    const auto totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

    auto groupBuffer = new char[maxThreads * frameSize];

    AVIOutputStream aviOutputStream(_arguments.output);

    (&aviOutputStream)
            ->setFps(_arguments.fps)
            ->setSize(_arguments.size)
            ->setTotalFrames(totalFrames);

    color::YCbCr444 *yuvFrameBuffer[maxThreads];
    for(int i = 0; i < maxThreads; i++)
        yuvFrameBuffer[i] = new color::YCbCr444(this->_cachedPaddingSize);  
    // color::YCbCr444 yuvFrameBuffer(this->_cachedPaddingSize);

    // share outputBuffer to reduce re-alloc
    vector<char> outputBuffer[maxThreads];

    // init local variables
    for(auto i = 0; i < maxThreads; i++) {
        // 2 MB
        outputBuffer[i].reserve(2 * 1024 * 1024);
    }

    aviOutputStream.start();
 
    auto totalSeconds = Utils::getCurrentTimestamp(
            totalFrames, totalFrames,
            _arguments.fps
    );
    auto totalTimeStr = Utils::formatTimestamp(totalSeconds);
    cout << endl;

    for (size_t frameNo = 0; frameNo < totalFrames; frameNo += maxThreads) {
        int readCnt = videoReader.readFrame(frameNo, groupBuffer, maxThreads);
        #pragma omp parallel for default(none) \
            shared( \
                totalFrames, groupBuffer, outputBuffer, \
                readCnt, yuvFrameBuffer, \
                maxThreads, frameSize, \
                totalPixels, aviOutputStream, totalTimeStr, cout \
            ) schedule(static, 1)
        for(int i = 0 ; i < readCnt; i++){
            int tid = omp_get_thread_num();

            void *passData[] = {
                yuvFrameBuffer[tid],
                nullptr,
            };

            size_t bufferOffset = frameSize * i;

            this->encodeJpeg(
                    (color::RGBA*)(groupBuffer + bufferOffset), totalPixels,
                    outputBuffer[tid],
                    passData
            );
        }

        // cout << _arguments.tmpDir << endl;
        for(int tid = 0 ; tid < readCnt; tid++){
            if (_writeIntermediateResult) {
                writeBuffer(
                        _arguments.tmpDir + "/output-" + to_string(frameNo+tid) + ".jpg",
                        outputBuffer[tid].data(), outputBuffer[tid].size()
                );
            }
            aviOutputStream.writeFrame(outputBuffer[tid].data(), outputBuffer[tid].size());
        }
        auto currentTime = Utils::getCurrentTimestamp(frameNo + readCnt - 1, videoReader.getTotalFrames(), _arguments.fps);
        auto currentTimeStr = Utils::formatTimestamp(currentTime);

        cout << "\u001B[A" << std::flush
           << "Time: " << Utils::formatTimestamp(currentTime) << " / " << totalTimeStr
           << endl;
    }

    cout << "\u001B[A" << std::flush
         << "Time: " << totalTimeStr << " / " << totalTimeStr
         << endl;
    aviOutputStream.close();

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;

    for (int i = 0; i < maxThreads; i++) {
        delete yuvFrameBuffer[i];
    }
    delete[] groupBuffer;
}

void MJPEGEncoderOpenMPImpl::finalize() {

}

void MJPEGEncoderOpenMPImpl::encodeJpeg(color::RGBA *originalData, int length, vector<char> &output,
                                        void **sharedData) {
    auto yuvFrameBuffer = static_cast<color::YCbCr444 *>(sharedData[0]);
    transformColorSpace(originalData, *yuvFrameBuffer, this->_arguments.size, this->_cachedPaddingSize);

    // auto localBitBuffer = static_cast<BitBuffer *>(sharedData[1]);
    // localBitBuffer->init();

    BitBuffer localBitBuffer;
    localBitBuffer.init();

    output.assign(commonJpegHeader.begin(), commonJpegHeader.end());

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
    const auto maxWidth = (this->_cachedPaddingSize).width;
    const auto maxHeight = (this->_cachedPaddingSize).height;
    float Y[8][8], Cb[8][8], Cr[8][8];
    vector<BitCode> YBitCodeBuffer, CbBitCodeBuffer, CrBitCodeBuffer;
    vector<int> YSize, CbSize, CrSize;

    for (auto mcuY = 0; mcuY < maxHeight; mcuY += 8) { // each step is either 8 or 16 (=mcuSize)
        for (auto mcuX = 0; mcuX < maxWidth; mcuX += 8) {
            for (auto deltaY = 0; deltaY < 8; ++deltaY) {
                const auto row = mcuY + deltaY;
                for (auto deltaX = 0; deltaX < 8; ++deltaX) {
                    const auto pixelPos = row * maxWidth + deltaX + mcuX;

                    Y[deltaY][deltaX] = yuvFrameBuffer->getYChannel()[pixelPos];
                    Cb[deltaY][deltaX] = yuvFrameBuffer->getCbChannel()[pixelPos];
                    Cr[deltaY][deltaX] = yuvFrameBuffer->getCrChannel()[pixelPos];
                }
            }
            lastYDC =  encodeBlock(output, Y, _scaledLuminance, lastYDC, _huffmanLuminanceDC, _huffmanLuminanceAC, codewords, localBitBuffer);
            lastCbDC = encodeBlock(output, Cb, _scaledChrominance, lastCbDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords, localBitBuffer);
            lastCrDC = encodeBlock(output, Cr, _scaledChrominance, lastCrDC, _huffmanChrominanceDC, _huffmanChrominanceAC, codewords, localBitBuffer);

        } // end mcuX
    } // end mcuY

    writeBitCode(output, BitCode(0x7F, 7), localBitBuffer);
    
    output.emplace_back(0xFF);
    output.emplace_back(0xD9);
}

int16_t MJPEGEncoderOpenMPImpl::encodeBlock(vector<char>& output, float block[8][8], const float scaled[8*8], int16_t lastDC,
                    const BitCode huffmanDC[256], const BitCode huffmanAC[256], const BitCode* codewords, BitBuffer& bitBuffer)
{
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

    // same "average color" as previous block ?
    auto diff = DC - lastDC;
    if (diff == 0)
        writeBitCode(output, huffmanDC[0x00], bitBuffer);
    else
    {
        auto& bits = codewords[diff]; // nope, encode the difference to previous block's average color
        writeBitCode(output, huffmanDC[bits.numBits], bitBuffer);
        writeBitCode(output, bits, bitBuffer);
    }

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
                writeBitCode(output, huffmanAC[0xF0], bitBuffer);
                // bitCodeBuffer.emplace_back(huffmanAC[0xF0]);
                offset = 0;
            }
            i++;
        }

        auto& encoded = codewords[quantized[i]];
        // combine number of zeros with the number of bits of the next non-zero value
        writeBitCode(output, huffmanAC[offset + encoded.numBits], bitBuffer);
        writeBitCode(output, encoded, bitBuffer);
        offset = 0;
    }

    // send end-of-block code (0x00), only needed if there are trailing zeros
    if (posNonZero < 8*8 - 1) // = 63
        writeBitCode(output, huffmanAC[0x00], bitBuffer);
    return DC;
}