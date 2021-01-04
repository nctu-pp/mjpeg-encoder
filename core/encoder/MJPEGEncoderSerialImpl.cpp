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

void MJPEGEncoderSerialImpl::start() {
    RawVideoReader videoReader(_arguments.input, _arguments.size);
    auto totalFrames = videoReader.getTotalFrames();
    const auto totalPixels = this->_cachedPaddingSize.height * this->_cachedPaddingSize.width;

    size_t paddedRgbFrameSize = totalPixels * RawVideoReader::PIXEL_BYTES;

    auto buffer = new char[videoReader.getPerFrameSize()];
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

    BitBuffer bitBuffer;

    aviOutputStream.start();

    void *passData[] = {
            &yuvFrameBuffer,
            yBlock,
            cbBlock,
            crBlock,
            &bitBuffer,
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
    if (this->_arguments.showMeasure) {
        cout << "PaddingAndTransformColorSpace Time " << this->_paddingAndTransformColorSpaceTime << endl
                << "DctAndQuantization Time " << this->_dctAndQuantizationTime << endl
                << "Encode Time " << this->_encodeTime << endl;
    }

    cout << endl
         << "Video encoded, output file located at " << _arguments.output
         << endl;
    delete[] buffer;
    delete[] yBlock;
    delete[] cbBlock;
    delete[] crBlock;
}

void MJPEGEncoderSerialImpl::finalize() {

}
