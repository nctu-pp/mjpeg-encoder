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

        int readFrame(ifstream& fs, char *buffer, int numOfFrame = 1) const;

        int readFrame(char *buffer, int numOfFrame = 1, int startAtFrame = 0);

        size_t getTotalFrames() const;

        size_t getPerFrameSize() const;

        ifstream openFile() const;

        static const size_t PIXEL_BYTES = 4;
    private:
        string _filePath;
        Size _size{0, 0};
        size_t _cachedTotalFrames = 0;
        size_t _cachedPerFrameSize = 0;

        void openFile(ifstream &fs, const size_t &pos = 0) const;

        void initCache();
    };
}
