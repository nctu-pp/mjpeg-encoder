//
// Created by wcl on 2020/11/05.
//
#include <algorithm>

#include "Model.h"

using namespace model;
using namespace model::color;

ImplKind model::parseImplKind(const string &str) {
    string lcStr(str);
    std::transform(lcStr.begin(), lcStr.end(), lcStr.begin(), ::tolower);

    if (lcStr == "openmp")
        return OpenMP;
    if (lcStr == "opencl")
        return OpenCL;

    return Serial;
}

YCbCr444::YCbCr444(const Size &size) {
    _size = size;
    _perChannelSize = size.height * size.width;

    _yChannel = new ChannelData[_perChannelSize];
    _cbChannel = new ChannelData[_perChannelSize];
    _crChannel = new ChannelData[_perChannelSize];
}

YCbCr444::~YCbCr444() {
    delete[] _yChannel;
    delete[] _cbChannel;
    delete[] _crChannel;

    _yChannel = nullptr;
    _cbChannel = nullptr;
    _crChannel = nullptr;
}

YCbCr444::ChannelData *__restrict YCbCr444::getYChannel() const {
    return _yChannel;
}

YCbCr444::ChannelData *__restrict YCbCr444::getCbChannel() const {
    return _cbChannel;
}

YCbCr444::ChannelData *__restrict YCbCr444::getCrChannel() const {
    return _crChannel;
}

size_t YCbCr444::getPerChannelSize() const {
    return _perChannelSize;
}
