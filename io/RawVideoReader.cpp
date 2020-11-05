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

int io::RawVideoReader::readFrame(int seek, char *buffer, int numOfFrame) {
    // return zero if eof
    return 0;
}

size_t io::RawVideoReader::getTotalFrames() const {

    return _cachedTotalFrames;
}

size_t io::RawVideoReader::getPerFrameSize() const {
    return 0;
}

void io::RawVideoReader::openFile(ifstream &fs, const size_t &pos) {
    fs.open(this->_filePath, ifstream::in | ifstream::binary);
    fs.seekg(pos, ios::beg);
}

void io::RawVideoReader::initCache() {
    ifstream fs;
    openFile(fs);

    fs.seekg(0, ios_base::end);

    size_t size = fs.tellg();

    fs.close();

    _cachedPerFrameSize = _size.width * _size.height * 4;
    _cachedTotalFrames = size / _cachedPerFrameSize;
}
