//
// Created by wcl on 2020/11/01.
//

#pragma once

#include <iostream>
#include <string>
#include <fstream>

#include "../model/Model.h"

using namespace std;
using namespace model;

namespace io {
    class RawVideoReader {
    public:
        explicit RawVideoReader(const string &filePath, const Size &size);

        ~RawVideoReader();

        int readFrame(char *buffer, int numOfFrame = 1);

        int readFrame(int startAtFrame, char *buffer, int numOfFrame = 1);

        size_t getTotalFrames() const;

        size_t getPerFrameSize() const;

        static const size_t PIXEL_BYTES = 4;
    private:
        ifstream _fs;
        string _filePath;
        Size _size{0, 0};
        size_t _cachedTotalFrames = 0;
        size_t _cachedPerFrameSize = 0;

        void initFileStream();
    };
}
