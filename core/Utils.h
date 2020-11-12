//
// Created by wcl on 2020/11/12.
//

#pragma once

#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

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
