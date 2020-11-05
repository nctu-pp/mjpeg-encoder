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

        int readFrame(int seek, char *buffer, int numOfFrame = 1);

        size_t getTotalFrames() const;

        size_t getPerFrameSize() const;

    private:
        string _filePath;
        Size _size;
        size_t _cachedTotalFrames;
        size_t _cachedPerFrameSize;

        void openFile(ifstream& fs, const size_t& pos = 0);

        void initCache();
    };
}
