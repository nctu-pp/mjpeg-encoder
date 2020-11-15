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

}

void MJPEGEncoderOpenMPImpl::start() {

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
    AbstractMJPEGEncoder::transformColorSpace(rgbaBuffer, yuv444Buffer, frameSize);
}
