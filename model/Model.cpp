//
// Created by wcl on 2020/11/05.
//
#include <algorithm>

#include "Model.h"

using namespace model;

ImplKind model::parseImplKind(const string &str) {
    string lcStr(str);
    std::transform(lcStr.begin(), lcStr.end(), lcStr.begin(), ::tolower);

    if (lcStr == "openmp")
        return OpenMP;
    if (lcStr == "opencl")
        return OpenCL;

    return Serial;
}
