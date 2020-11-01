//
// Created by wcl on 2020/11/01.
//

#pragma once

#include <iostream>
#include <string>
#include <fstream>

#include "../model/Color.h"
#include "../model/Size.h"

using namespace std;
using namespace model;

namespace io {
    class RawVideoReader {
    public:
        explicit RawVideoReader(const string &filePath, const Size &size);

        int readFrame(char *buffer, int numOfFrame = 1);

        void seek(int newPosition);

        int getCurrentPosition() const;

        bool eof() const;

    private:
        string _filePath;
        Size _size;
        int _currentPosition;
    };
}
