//
// Created by wcl on 2020/11/01.
//

#include "AVIOutputStream.h"

core::AVIOutputStream::AVIOutputStream(const string &path) {
    _path = path;
    _started = false;
    _closed = false;
    this->_outStream.open(path.c_str());
}

core::AVIOutputStream *core::AVIOutputStream::setSize(const Size &size) {
    _size = size;
    return this;
}

core::AVIOutputStream *core::AVIOutputStream::setFps(int fps) {
    _fps = fps;
    return this;
}

core::AVIOutputStream *core::AVIOutputStream::setTotalFrames(size_t numOfFrames) {
    _numOfFrames = numOfFrames;
    return this;
}

void core::AVIOutputStream::fwrite_DWORD(DWORD word) {
  unsigned char *p;
  p = (unsigned char *)&word;
  for (int i = 0; i < 4; i++) {
    this->_outStream << (unsigned char)(p[i]);
  }
}

void core::AVIOutputStream::fwrite_WORD(WORD word) {
  unsigned char *p;
  p = (unsigned char *)&word;
  for (int i = 0; i < 2; i++) {
    this->_outStream << (unsigned char)p[i]; 
  }
}

void core::AVIOutputStream::fwrite_word(string word) {
  for (int i = 0; i < 4; i++) {
    this->_outStream << (unsigned char)word[i];
  }
}

void core::AVIOutputStream::start() {
    _started = true;

    RIFF RIFF_LIST;

    // RIFF_LIST.dwRIFF = 'RIFF';
    fwrite_word("RIFF");
    DWORD len = 0;

    RIFF_LIST.dwSize = 150 + 12 + len + 8 * _numOfFrames + 8 + 4 * 4 * _numOfFrames;
    fwrite_DWORD(RIFF_LIST.dwSize);

    // RIFF_LIST.dwFourCC = 'AVI ';
    fwrite_word("AVI ");

    LIST hdrl;
    // hdrl.dwList = 'LIST';
    fwrite_word("LIST");

    hdrl.dwSize = 208;
    fwrite_DWORD(hdrl.dwSize);
    // hdrl.dwFourCC = 'hdrl';
    fwrite_word("hdrl");

    MainAVIHeader avih;

    // avih.dwFourCC = 'avih';
    fwrite_word("avih");
    avih.dwSize = 56;
    fwrite_DWORD(avih.dwSize);

    avih.dwMicroSecPerFrame = 1000000 / _fps;
    fwrite_DWORD(avih.dwMicroSecPerFrame);

    avih.dwMaxBytesPerSec = 7000;
    fwrite_DWORD(avih.dwMaxBytesPerSec);

    avih.dwPaddingGranularity = 0;
    fwrite_DWORD(avih.dwPaddingGranularity);

    // dwFlags set to 16, do not know why!
    avih.dwFlags = 16;
    fwrite_DWORD(avih.dwFlags);

    avih.dwTotalFrames = _numOfFrames;
    fwrite_DWORD(avih.dwTotalFrames);

    avih.dwInitialFrames = 0;
    fwrite_DWORD(avih.dwInitialFrames);

    avih.dwStreams = 1;
    fwrite_DWORD(avih.dwStreams);

    avih.dwSuggestedBufferSize = 0;
    fwrite_DWORD(avih.dwSuggestedBufferSize);

    avih.dwWidth = _size.width;
    fwrite_DWORD(avih.dwWidth);

    avih.dwHeight = _size.height;
    fwrite_DWORD(avih.dwHeight);

    avih.dwReserved[0] = 0;
    fwrite_DWORD(avih.dwReserved[0]);
    avih.dwReserved[1] = 0;
    fwrite_DWORD(avih.dwReserved[1]);
    avih.dwReserved[2] = 0;
    fwrite_DWORD(avih.dwReserved[2]);
    avih.dwReserved[3] = 0;
    fwrite_DWORD(avih.dwReserved[3]);

    LIST strl;
    // strl.dwList = 'LIST';
    fwrite_word("LIST");
    strl.dwSize = 132;
    fwrite_DWORD(strl.dwSize);

    // strl.dwFourCC = 'strl';
    fwrite_word("strl");
    AVIStreamHeader strh;
    // strh.dwFourCC = 'strh';
    fwrite_word("strh");
    strh.dwSize = 48;
    fwrite_DWORD(strh.dwSize);
    // strh.fccType = 'vids';
    fwrite_word("vids");
    // strh.fccHandler = 'MJPG';
    fwrite_word("MJPG");
    strh.dwFlags = 0;
    fwrite_DWORD(strh.dwFlags);
    strh.wPriority = 0; // +2 = 14
    fwrite_WORD(strh.wPriority);
    strh.wLanguage = 0; // +2 = 16
    fwrite_WORD(strh.wLanguage);
    strh.dwInitialFrames = 0; // +4 = 20
    fwrite_DWORD(strh.dwInitialFrames);
    strh.dwScale = 1; // +4 = 24
    fwrite_DWORD(strh.dwScale);
    // insert FPS
    strh.dwRate = _fps; // +4 = 28
    fwrite_DWORD(strh.dwRate);
    strh.dwStart = 0; // +4 = 32
    fwrite_DWORD(strh.dwStart);
    // insert nbr of jpegs
    strh.dwLength = _numOfFrames; // +4 = 36
    fwrite_DWORD(strh.dwLength);

    strh.dwSuggestedBufferSize = 0; // +4 = 40
    fwrite_DWORD(strh.dwSuggestedBufferSize);
    strh.dwQuality = 0; // +4 = 44
    fwrite_DWORD(strh.dwQuality);
    strh.dwSampleSize = 0; // +4 = 48
    fwrite_DWORD(strh.dwSampleSize);

    EXBMINFOHEADER strf;

    // strf.dwFourCC = 'strf';
    fwrite_word("strf");
    strf.dwSize = 40;
    fwrite_DWORD(strf.dwSize);

    strf.biSize = 40;
    fwrite_DWORD(strf.biSize);

    strf.biWidth = _size.width;
    fwrite_DWORD(strf.biWidth);
    strf.biHeight = _size.height;
    fwrite_DWORD(strf.biHeight);
    strf.biPlanes = 1;
    fwrite_WORD(strf.biPlanes);
    strf.biBitCount = 24;
    fwrite_WORD(strf.biBitCount);
    // strf.biCompression = 'MJPG';
    fwrite_word("MJPG");
    strf.biSizeImage =
        ((strf.biWidth * strf.biBitCount / 8 + 3) & 0xFFFFFFFC) * strf.biHeight;
    fwrite_DWORD(strf.biSizeImage);
    strf.biXPelsPerMeter = 0;
    fwrite_DWORD(strf.biXPelsPerMeter);
    strf.biYPelsPerMeter = 0;
    fwrite_DWORD(strf.biYPelsPerMeter);
    strf.biClrUsed = 0;
    fwrite_DWORD(strf.biClrUsed);
    strf.biClrImportant = 0;
    fwrite_DWORD(strf.biClrImportant);

    fwrite_word("LIST");
    DWORD ddww = 16;
    fwrite_DWORD(ddww);
    fwrite_word("odml");
    fwrite_word("dmlh");
    DWORD szs = 4;
    fwrite_DWORD(szs);

    DWORD totalframes = _numOfFrames;
    fwrite_DWORD(totalframes);

    LIST movi;
    // movi.dwList = 'LIST';
    fwrite_word("LIST");
    movi.dwSize = len + 4 + 8 * _numOfFrames;
    fwrite_DWORD(movi.dwSize);
    // movi.dwFourCC = 'movi';
    fwrite_word("movi");
}

void core::AVIOutputStream::writeFrame(const char *data, int len) {
    CHUNK dataChunk;
    //data.dwFourCC = '00db';
    fwrite_word("00db");
    dataChunk.dwSize = len;
    fwrite_DWORD(dataChunk.dwSize);

    int l = len;
    //put image into avi containter
    while (l--) {
      this->_outStream << (unsigned char)(*data);
      data++;
    }

    if (len % 2) {
      this->_outStream << '\0';
    }

    if (len % 2) {
      len += 1;
    }

    _lenVec.push_back(len);
    _offsetVec.push_back(_offsetCount);
    _offsetCount += len + 8;
}


void core::AVIOutputStream::close() {
    fwrite_word("idx1");
    unsigned long index_length = 4 * 4 * _numOfFrames;
    const unsigned long AVI_KEYFRAME = 16;
    fwrite_DWORD(index_length);
    int n = 0;
    while(n < _numOfFrames){
        fwrite_word("00db");
        fwrite_DWORD(AVI_KEYFRAME);
        fwrite_DWORD(_offsetVec[n]);
        fwrite_DWORD(_lenVec[n]);
        n++;
    }
    _outStream.close();
    _closed = true;
}
