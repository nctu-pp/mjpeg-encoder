//
// Created by wcl on 2020/11/05.
//

#pragma once

#include <string>

using namespace std;

namespace model {
    typedef struct Size_s {
        unsigned int height;
        unsigned int width;
    } Size;

    namespace color {
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

    typedef enum ImplKind_e {
        Serial,
        OpenMP,
        OpenCL,
    } ImplKind;

    typedef struct Arguments_s {
        string input;
        string output;
        Size size{0, 0};
        int fps = 0;
        int numThreads = 0;
        ImplKind kind = Serial;
    } Arguments;

    ImplKind parseImplKind(const string& str);
}
