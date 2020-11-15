//
// Created by wcl on 2020/11/01.
//

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include "../model/Model.h"

#pragma once

using namespace std;
using namespace model;

namespace core {
    class AVIOutputStream {
    public:
        explicit AVIOutputStream(const string &path);

        AVIOutputStream *setSize(const Size &size);

        AVIOutputStream *setFps(int fps);

        // remove this if avi container do not need this info
        AVIOutputStream *setTotalFrames(size_t numOfFrames);

        void fwrite_DWORD(DWORD word);

        void fwrite_WORD(WORD word);
        
        void fwrite_word(const char* word);

        void start();

        void writeFrame(const char *data, int len);

        void close();

    private:
        string _path;
        ofstream _outStream;
        bool _started;
        bool _closed;
        size_t _numOfFrames;
        int _fps;
        Size _size;
        vector<unsigned long> _lenVec;
        vector<unsigned long> _offsetVec;
        unsigned long _offsetCount;

        void writeHeader();
    };
}
