#!/bin/bash
mkdir -p -m=700 cmake-build-pi
pushd cmake-build-pi

if [ "$1" != "run" ]
then
  cmake -DCMAKE_C_COMPILER=$(which clang-11) -DCMAKE_CXX_COMPILER=$(which clang++-11) \
    -DCMAKE_BUILD_TYPE=Release ../ \
    -DOpenCL_INCLUDE_DIR=/usr/local/cuda/include/ -DOpenCL_LIBRARY=/usr/local/cuda/lib64/libOpenCL.so.1.1 \
  # -DOpenCL_INCLUDE_DIR=/opt/AMDAPPSDK-3.0/include/ -DOpenCL_LIBRARY=/usr/lib/x86_64-linux-gnu/libOpenCL.so.1.0.0

  make -j8  opencl-pi
else
  LD_PRELOAD=~/lib/libhook_amd_opencl.so ./opencl-pi $2
  # LD_PRELOAD=/home/ubuntu/lib/libhook_nvidia_opencl.so
fi
