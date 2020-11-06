//
// Created by wcl on 2020/11/05.
//
#include <algorithm>

#include "Model.h"

using namespace model;

ImplKind model::parseImplKind(const string &str) {
    string lcStr(str);
    std::transform(lcStr.begin(), lcStr.end(), lcStr.begin(), ::tolower);

    if (lcStr == "openmp")
        return OpenMP;
    if (lcStr == "opencl")
        return OpenCL;

    return Serial;
}

color::YCbCr444::YCbCr444(const Size &size) {
    _size = size;
    _perChannelSize = size.height * size.width;

    _yChannel = new char[_perChannelSize];
    _cbChannel = new char[_perChannelSize];
    _crChannel = new char[_perChannelSize];
}

color::YCbCr444::~YCbCr444() {
    delete[] _yChannel;
    delete[] _cbChannel;
    delete[] _crChannel;

    _yChannel = nullptr;
    _cbChannel = nullptr;
    _crChannel = nullptr;
}

char *__restrict color::YCbCr444::getYChannel() const {
    return _yChannel;
}

char *__restrict color::YCbCr444::getCbChannel() const {
    return _cbChannel;
}

char *__restrict color::YCbCr444::getCrChannel() const {
    return _crChannel;
}

size_t color::YCbCr444::getPerChannelSize() const {
    return _perChannelSize;
}
