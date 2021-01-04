//
// Created by wcl on 2020/11/12.
//

#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace std;

#define TEST_TIME_START(name) std::chrono::steady_clock::time_point __##name##_begin = std::chrono::steady_clock::now()
#define TEST_TIME_END(name) (std::chrono::duration_cast<std::chrono::microseconds>((std::chrono::steady_clock::now() - __##name##_begin)).count())

namespace core {
    class Utils {
    public:
        static float getCurrentTimestamp(
                const unsigned int &currentFrame, const unsigned int &totalFrames,
                const unsigned int &fps
        );

        static string formatTimestamp(float seconds);

        static ostream& printProgress(ostream& o, const string& str);
    };
}
