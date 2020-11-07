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
