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
                unsigned char a;
            } color;

            unsigned int value;
        } RGBA;

        class YCbCr444 {
        public:
            typedef unsigned char ChannelData;

            explicit YCbCr444(const Size &size);

            ~YCbCr444();

            /**
             * Get Y Channel pointer, [0, 255]
             * @return data pointer
             */
            ChannelData *__restrict getYChannel() const;

            /**
             * Get Cb (U) Channel pointer, [0, 255]
             * @return data pointer
             */
            ChannelData *__restrict getCbChannel() const;

            /**
             * Get Cr (V) Channel pointer, [0, 255]
             * @return data pointer
             */
            ChannelData *__restrict getCrChannel() const;

            size_t getPerChannelSize() const;


            YCbCr444(const YCbCr444 &) = delete;

        private:
            Size _size{0, 0};
            size_t _perChannelSize = 0;
            ChannelData *__restrict _yChannel = nullptr;
            ChannelData *__restrict _cbChannel = nullptr;
            ChannelData *__restrict _crChannel = nullptr;
        };
    }

    struct BitCode
    {
        BitCode() = default; // undefined state, must be initialized at a later time
        BitCode(uint16_t code_, uint8_t numBits_)
        : code(code_), numBits(numBits_) {}
        uint16_t code;       // JPEG's Huffman codes are limited to 16 bits
        uint8_t  numBits;    // number of valid bits
    };

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

    // under struct is for avi container
    typedef uint32_t DWORD;
    typedef uint32_t LONG;
    typedef uint16_t WORD;
    typedef uint8_t BYTE;

    typedef struct {
        DWORD dwRIFF;
        DWORD dwSize;
        DWORD dwFourCC;
    } RIFF;

    typedef struct {
        DWORD dwFourCC;
        DWORD dwSize;
        //	BYTE* data; // dwSize in length
    } CHUNK;

    typedef struct {
        DWORD dwList;
        DWORD dwSize;
        DWORD dwFourCC;
        //	BYTE* data; // dwSize - 4 in length
    } LIST;

    typedef struct {
        DWORD dwFourCC;
        DWORD dwSize;

        DWORD dwMicroSecPerFrame;
        DWORD dwMaxBytesPerSec;
        DWORD dwPaddingGranularity;

        DWORD dwFlags;
        DWORD dwTotalFrames;
        DWORD dwInitialFrames;
        DWORD dwStreams;
        DWORD dwSuggestedBufferSize;

        DWORD dwWidth;
        DWORD dwHeight;

        DWORD dwReserved[4];
    } MainAVIHeader;

    typedef struct _RECT {
        LONG left;
        LONG top;
        LONG right;
        LONG bottom;
    } RECT;

    typedef struct {
        DWORD dwFourCC;
        DWORD dwSize;

        DWORD fccType;
        DWORD fccHandler;
        DWORD dwFlags;
        WORD  wPriority;
        WORD  wLanguage;
        DWORD dwInitialFrames;
        DWORD dwScale;
        DWORD dwRate;
        DWORD dwStart;
        DWORD dwLength;
        DWORD dwSuggestedBufferSize;
        DWORD dwQuality;
        DWORD dwSampleSize;

    } AVIStreamHeader;

    typedef struct {
        DWORD dwFourCC;
        DWORD dwSize;

        DWORD biSize;
        DWORD  biWidth;
        DWORD  biHeight;
        WORD  biPlanes;
        WORD  biBitCount;
        DWORD biCompression;
        DWORD biSizeImage;
        DWORD biXPelsPerMeter;
        DWORD  biYPelsPerMeter;
        DWORD biClrUsed;
        DWORD biClrImportant;
    } BITMAPINFOHEADER;

    typedef struct tagEXBMINFOHEADER {
        DWORD dwFourCC;
        DWORD dwSize;

        DWORD biSize;
        LONG  biWidth;
        LONG  biHeight;
        WORD  biPlanes;
        WORD  biBitCount;
        DWORD biCompression;
        DWORD biSizeImage;
        LONG  biXPelsPerMeter;
        LONG  biYPelsPerMeter;
        DWORD biClrUsed;
        DWORD biClrImportant;

    } EXBMINFOHEADER;

    typedef struct {
        DWORD ckid;
        DWORD dwFlags;
        DWORD dwChunkOffset;
        DWORD dwChunkLength;
    } AVIINDEXENTRY;

    typedef struct {
        DWORD fcc;
        DWORD cd;
        WORD wLongsPerEntry;
        char bIndexSubType;
        char bIndexType;
        DWORD nEntriesInUse;
        DWORD dwChunkId;
        DWORD dwReserved[3];
        AVIINDEXENTRY axiindex_entry;
    } AVIINDEXCHUNK;

    typedef struct {
        DWORD name;
        DWORD dwSize;
        DWORD dwTotalFrames;
    } ODMLExtendedAVIheader;

    typedef struct {
        RIFF riff_AVI;
        LIST hdrl;
        MainAVIHeader avih;
        LIST strl;
        AVIStreamHeader strh;
        EXBMINFOHEADER strf;
        LIST odml;
        ODMLExtendedAVIheader dmlh;
        LIST movi;
        CHUNK movi_data;
    } avi_file;
}
