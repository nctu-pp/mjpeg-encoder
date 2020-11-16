//
// Created by wcl on 2020/11/05.
//

#pragma once

#include <memory>
#include <memory.h>
#include <fstream>
#include <vector>

#include "../../model/Model.h"
#include "../../io/RawVideoReader.h"
#include "../AVIOutputStream.h"

using namespace std;
using namespace model;

namespace core::encoder {

    class AbstractMJPEGEncoder {
    public:
        virtual void start() = 0;

        virtual void finalize() = 0;

        static shared_ptr<AbstractMJPEGEncoder> getInstance(const Arguments &arguments);

    protected:
        explicit AbstractMJPEGEncoder(const Arguments &arguments);

        virtual void encodeJpeg(
                color::RGBA *paddedData, int length, int quality,
                vector<char> &output,
                void **sharedData
        ) = 0;

        inline void encodeJpeg(
                color::RGBA *paddedData, int length, int quality,
                vector<char> &output
        ) {
            return encodeJpeg(
                    paddedData, length, quality,
                    output,
                    nullptr
            );
        }

        void calcPaddingSize();

        virtual void transformColorSpace(
                color::RGBA *__restrict rgbaBuffer, color::YCbCr444 &yuv444Buffer,
                const Size &frameSize
        ) const;

        Size getPaddingSize() const;

        Arguments _arguments;
        Size _cachedPaddingSize{0, 0};

        virtual void doPadding(
                char *__restrict originalBuffer, const Size &originalSize,
                char *__restrict targetBuffer, const Size &targetSize
        );

        static void writeBuffer(const string &path, char *buffer, size_t len, bool append = false);

        const static size_t BLOCK_SIZE = 8;
    
        inline int clamp(
                int value,
                int minValue,
                int maxValue
        ) const {
                if (value <= minValue) return minValue;
                if (value >= maxValue) return maxValue;
                return value;
        }
        
        const uint8_t DefaultQuantLuminance[8*8] =
                { 16, 11, 10, 16, 24, 40, 51, 61, // there are a few experts proposing slightly more efficient values,
                12, 12, 14, 19, 26, 58, 60, 55, // e.g. https://www.imagemagick.org/discourse-server/viewtopic.php?t=20333
                14, 13, 16, 24, 40, 57, 69, 56, // btw: Google's Guetzli project optimizes the quantization tables per image
                14, 17, 22, 29, 51, 87, 80, 62,
                18, 22, 37, 56, 68,109,103, 77,
                24, 35, 55, 64, 81,104,113, 92,
                49, 64, 78, 87,103,121,120,101,
                72, 92, 95, 98,112,100,103, 99 };

        const uint8_t DefaultQuantChrominance[8*8] =
                { 17, 18, 24, 47, 99, 99, 99, 99,
                18, 21, 26, 66, 99, 99, 99, 99,
                24, 26, 56, 99, 99, 99, 99, 99,
                47, 66, 99, 99, 99, 99, 99, 99,
                99, 99, 99, 99, 99, 99, 99, 99,
                99, 99, 99, 99, 99, 99, 99, 99,
                99, 99, 99, 99, 99, 99, 99, 99,
                99, 99, 99, 99, 99, 99, 99, 99 };

        const uint8_t ZigZagInv[8*8] =
                {  0, 1, 8,16, 9, 2, 3,10,   // ZigZag[] =  0, 1, 5, 6,14,15,27,28,
                17,24,32,25,18,11, 4, 5,   //             2, 4, 7,13,16,26,29,42,
                12,19,26,33,40,48,41,34,   //             3, 8,12,17,25,30,41,43,
                27,20,13, 6, 7,14,21,28,   //             9,11,18,24,31,40,44,53,
                35,42,49,56,57,50,43,36,   //            10,19,23,32,39,45,52,54,
                29,22,15,23,30,37,44,51,   //            20,22,33,38,46,51,55,60,
                58,59,52,45,38,31,39,46,   //            21,34,37,47,50,56,59,61,
                53,60,61,54,47,55,62,63 }; //            35,36,48,49,57,58,62,63

        const uint8_t DcLuminanceCodesPerBitsize[16]   = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };   // sum = 12
        const uint8_t DcLuminanceValues         [12]   = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes
        const uint8_t AcLuminanceCodesPerBitsize[16]   = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 }; // sum = 162
        const uint8_t AcLuminanceValues        [162]   =                                        // => 162 codes
                { 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08, // 16*10+2 symbols because
                0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28, // upper 4 bits can be 0..F
                0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59, // while lower 4 bits can be 1..A
                0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89, // plus two special codes 0x00 and 0xF0
                0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6, // order of these symbols was determined empirically by JPEG committee
                0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
                0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };

        const uint8_t DcChrominanceCodesPerBitsize[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };   // sum = 12
        const uint8_t DcChrominanceValues         [12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes (identical to DcLuminanceValues)
        const uint8_t AcChrominanceCodesPerBitsize[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 }; // sum = 162
        const uint8_t AcChrominanceValues        [162] =                                        // => 162 codes
                { 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91, // same number of symbol, just different order
                0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26, // (which is more efficient for AC coding)
                0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,
                0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
                0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,
                0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
                0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };

        const float AanScaleFactors[8] = { 1, 1.387039845f, 1.306562965f, 1.175875602f, 1, 0.785694958f, 0.541196100f, 0.275899379f };
        
        BitCode huffmanLuminanceDC[256];
        BitCode huffmanLuminanceAC[256];

        BitCode huffmanChrominanceDC[256];
        BitCode huffmanChrominanceAC[256];

        void DCT(
                float block[8*8],
                uint8_t stride
        ) const;

        void generateHuffmanTable(
                const uint8_t numCodes[16],
                const uint8_t* values,
                BitCode result[256]
        ) const;

        const int16_t CodeWordLimit = 2048; // +/-2^11, maximum value after DCT

        int16_t encodeBlock(
                vector<char>& output,
                float block[8][8],
                const float scaled[8*8],
                int16_t lastDC,
                const BitCode huffmanDC[256],
                const BitCode huffmanAC[256], 
                const BitCode* codewords
        );

        void addMarker(
                vector<char>& output,
                uint8_t id,
                uint16_t length
        );

        void writeBitCode(
                vector<char>& output,
                const BitCode& data
        );

        struct BitBuffer
        {
                int32_t data    = 0; // actually only at most 24 bits are used
                uint8_t numBits = 0; // number of valid bits (the right-most bits)
                void init() {
                        data = 0;
                        numBits = 0;
                }
        } _bitbuffer;

        void writeJFIFHeader(
                vector<char>& output
        ) const;

        void writeQuantizationTable(
                vector<char>& output,
                const uint8_t quantLuminance[64],
                const uint8_t quantChrominance[64]
        );

        void writeHuffmanTable(
                vector<char>& output             
        );

        void writeImageInfos(
                vector<char>& output
        );

        void writeScanInfo(
                vector<char>& output
        );
    };
}

