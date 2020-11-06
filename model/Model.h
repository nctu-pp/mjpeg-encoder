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
        typedef union alignas(4) RGBA_s {
            struct color_s {
                unsigned char b;
                unsigned char g;
                unsigned char r;
                [[maybe_unused]] unsigned char a;
            } color;

            [[maybe_unused]] unsigned int value;
        } RGBA;

        class YCbCr444 {
        public:
            explicit YCbCr444(const Size &size);

            ~YCbCr444();

            /**
             * Get Y Channel pointer, [0, 255]
             * @return data pointer
             */
            [[nodiscard]]
            char *__restrict getYChannel() const;

            /**
             * Get Cb (U) Channel pointer, [0, 255]
             * @return data pointer
             */
            [[nodiscard]]
            char *__restrict getCbChannel() const;

            /**
             * Get Cr (V) Channel pointer, [0, 255]
             * @return data pointer
             */
            [[nodiscard]]
            char *__restrict getCrChannel() const;

            [[nodiscard]]
            size_t getPerChannelSize() const;


            YCbCr444(const YCbCr444 &) = delete;

        private:
            Size _size{0, 0};
            size_t _perChannelSize = 0;
            char *__restrict _yChannel = nullptr;
            char *__restrict _cbChannel = nullptr;
            char *__restrict _crChannel = nullptr;
        };
    }

    typedef enum ImplKind_e {
        Serial,
        OpenMP,
        OpenCL,
    } ImplKind;

    typedef struct Arguments_s {
        string input;
        string output;
        string tmpDir;
        Size size{0, 0};
        int fps = 0;
        int quality = 0;
        int numThreads = 0;
        ImplKind kind = Serial;
    } Arguments;

    ImplKind parseImplKind(const string &str);
}
