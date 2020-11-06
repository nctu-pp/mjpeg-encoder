//
// Created by wcl on 2020/11/01.
//

#include "AVIOutputStream.h"

core::AVIOutputStream::AVIOutputStream(const string &path) {
    _path = path;
    _started = false;
    _closed = false;
}

core::AVIOutputStream *core::AVIOutputStream::setSize(const Size &size) {
    return this;
}

core::AVIOutputStream *core::AVIOutputStream::setFps(int fps) {
    return this;
}

core::AVIOutputStream *core::AVIOutputStream::setTotalFrames(size_t numOfFrames) {
    return this;
}

void core::AVIOutputStream::start() {
    _started = true;
}

void core::AVIOutputStream::writeFrame(char *data, int len) {

}

void core::AVIOutputStream::close() {
    _closed = true;
}
