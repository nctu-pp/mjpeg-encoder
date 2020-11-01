//
// Created by wcl on 2020/11/01.
//

#include "RawVideoReader.h"

io::RawVideoReader::RawVideoReader(const string &filePath, const Size &size) {
    _currentPosition = 0;
    _filePath = filePath;
    _size = size;
}

int io::RawVideoReader::readFrame(char *buffer, int numOfFrame) {
    // return zero if eof
    return 0;
}

void io::RawVideoReader::seek(int newPosition) {

}

int io::RawVideoReader::getCurrentPosition() const {
    return _currentPosition;
}

bool io::RawVideoReader::eof() const {
    return false;
}
