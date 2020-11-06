//
// Created by wcl on 2020/11/01.
//

#include "RawVideoReader.h"

io::RawVideoReader::RawVideoReader(const string &filePath, const Size &size) {
    _cachedTotalFrames = 0;
    _cachedPerFrameSize = 0;
    _filePath = filePath;
    _size = size;

    initCache();
}

size_t io::RawVideoReader::getTotalFrames() const {
    return _cachedTotalFrames;
}

size_t io::RawVideoReader::getPerFrameSize() const {
    return _cachedPerFrameSize;
}

void io::RawVideoReader::openFile(ifstream &fs, const size_t &pos) const {
    fs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fs.open(this->_filePath, ifstream::in | ifstream::binary);
    fs.seekg(pos, ios::beg);
}

void io::RawVideoReader::initCache() {
    ifstream fs;
    openFile(fs);

    fs.seekg(0, ios_base::end);

    size_t size = fs.tellg();

    fs.close();

    _cachedPerFrameSize = _size.width * _size.height * PIXEL_BYTES;
    _cachedTotalFrames = size / _cachedPerFrameSize;
}

int io::RawVideoReader::readFrame(ifstream &fs, char *buffer, int numOfFrame) const {
    int frameRead = 0;
    char *currentBufferAddr = buffer;
    while (frameRead < numOfFrame && !fs.eof()) {
        fs.read(currentBufferAddr, _cachedPerFrameSize);
        currentBufferAddr += _cachedPerFrameSize;

        frameRead++;
    }

    return frameRead;
}

int io::RawVideoReader::readFrame(char *buffer, int numOfFrame, int startAtFrame) {
    ifstream fs;
    openFile(fs, startAtFrame * _cachedPerFrameSize);

    return readFrame(fs, buffer, numOfFrame);
}

ifstream io::RawVideoReader::openFile() const {
    ifstream fs;
    openFile(fs, 0);

    return fs;
}
