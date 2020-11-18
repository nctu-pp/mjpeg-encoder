#!/bin/bash
mkdir -p -m=700 cmake-build-dist
pushd cmake-build-dist
cmake -DCMAKE_C_COMPILER=$(which clang-11) -DCMAKE_CXX_COMPILER=$(which clang++-11) -DCMAKE_BUILD_TYPE=Release ../
make -j8
popd
