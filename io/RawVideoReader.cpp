//
// Created by wcl on 2020/11/01.
//

#include "RawVideoReader.h"

io::RawVideoReader::RawVideoReader(const string &filePath, const Size &size) {
    _cachedTotalFrames = 0;
    _cachedPerFrameSize = 0;
    _filePath = filePath;
    _size = size;

    initFileStream();
}

void io::RawVideoReader::initFileStream() {
    auto defaultException = _fs.exceptions();
    _fs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    _fs.open(this->_filePath, ifstream::in | ifstream::binary);
    _fs.seekg(0, ios_base::end);

    size_t size = _fs.tellg();

    _cachedPerFrameSize = _size.width * _size.height * PIXEL_BYTES;
    _cachedTotalFrames = size / _cachedPerFrameSize;

    _fs.exceptions(defaultException);
    _fs.seekg(0, ios_base::beg);
}

size_t io::RawVideoReader::getTotalFrames() const {
    return _cachedTotalFrames;
}

size_t io::RawVideoReader::getPerFrameSize() const {
    return _cachedPerFrameSize;
}

int io::RawVideoReader::readFrame(char *buffer, int numOfFrame) {
    int frameRead = 0;
    char *currentBufferAddr = buffer;
    while (frameRead < numOfFrame && !_fs.eof()) {
        _fs.read(currentBufferAddr, _cachedPerFrameSize);
        currentBufferAddr += _cachedPerFrameSize;

        frameRead++;
    }

    return frameRead;
}

int io::RawVideoReader::readFrame(int startAtFrame, char *buffer, int numOfFrame) {
    _fs.seekg(startAtFrame * _cachedPerFrameSize, ios_base::beg);

    return readFrame(buffer, numOfFrame);
}

io::RawVideoReader::~RawVideoReader() {
    _fs.close();
}
