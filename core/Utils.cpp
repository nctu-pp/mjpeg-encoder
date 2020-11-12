//
// Created by wcl on 2020/11/12.
//

#include "Utils.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#define _isatty isatty
#define _fileno fileno
#endif

using namespace core;

float Utils::getCurrentTimestamp(
        const unsigned int &currentFrame, const unsigned &totalFrames,
        const unsigned int &fps
) {
    float secPerFrame = 1.f / (float) fps;
    return secPerFrame * (float) currentFrame;
}

string Utils::formatTimestamp(float seconds) {
    stringstream ss;
    const int base = 100;
    int ms = static_cast<int>(seconds * (float) base);
    int hBase = 60 * 60 * base;
    int mBase = 60 * base;
    int sBase = base;
    int msBase = 1;

    int tHours = ms / hBase;
    ms -= hBase * tHours;

    int tMinutes = ms / mBase;
    ms -= mBase * tMinutes;

    int tSeconds = ms / sBase;
    ms -= sBase * tSeconds;

    int tMs = ms / msBase;

    const int fieldSize = 2;
    ss << std::setw(fieldSize) << std::setfill('0') << tHours << ':'
       << std::setw(fieldSize) << std::setfill('0') << tMinutes << ':'
       << std::setw(fieldSize) << std::setfill('0') << tSeconds << '.'
       << std::setw(fieldSize) << std::setfill('0') << tMs;

    return ss.str();
}

ostream &Utils::printProgress(ostream &o, const string &str) {
    if(_isatty(_fileno(stdout)) == 1) {
        o << "\r" << str << std::flush;
    } else {
        static std::ios::off_type last(-1);
        if(last != -1)
            o.seekp(last, std::ios::beg);
        last = o.tellp();
        o << str << std::endl;
    }
    return o;
}
