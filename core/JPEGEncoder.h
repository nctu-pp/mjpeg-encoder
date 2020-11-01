//
// Created by wcl on 2020/11/01.
//


#include "../model/Color.h"
#include "../model/Size.h"

#pragma once

using namespace std;
using namespace model;

namespace core {
    class JPEGEncoder {
    public:
        explicit JPEGEncoder() = default;

        void encode(
                color::RGBA *data, int length, int quality,
                char *outBuffer, int &outLength
        ) const;

    private:

    };
}
