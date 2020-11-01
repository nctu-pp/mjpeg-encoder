//
// Created by wcl on 2020/11/01.
//

#pragma once

namespace model::color {
    typedef union RGBA_s {
        struct color_s {
            unsigned char b;
            unsigned char g;
            unsigned char r;
            unsigned char a;
        } color;

        unsigned int value;
    } RGBA;


}

