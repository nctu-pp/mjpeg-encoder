#!/bin/bash
targetDir=cmake-build-dist

if [ "$1" == "clean" ]; then
    rm -rf $targetDir
fi

mkdir -p -m=700 $targetDir
pushd $targetDir

cmake \
  -DCMAKE_C_COMPILER=$(which clang-11 clang | head -n1) \
  -DCMAKE_CXX_COMPILER=$(which clang++-11 clang++ | head -n1) \
  -DCMAKE_BUILD_TYPE=Release ../

make -j$(nproc --all)

if [ $? -eq 0 ]; then
  binPath=$(realpath mjpeg_encoder)
  echo -e "Build succeeded.\nExecutable Path: [$binPath]."
fi

popd > /dev/null
