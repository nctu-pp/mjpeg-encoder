//
// Created by wcl on 2020/11/01.
//

#include <string>
#include <iostream>
#include <fstream>

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

        void start();

        void writeFrame(char *data, int len);

        void close();

    private:
        string _path;
        ofstream _outStream;
        bool _started;
        bool _closed;

        void writeHeader();
    };
}
