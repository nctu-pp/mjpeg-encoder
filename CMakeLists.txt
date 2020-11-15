cmake_minimum_required(VERSION 3.17)
project(mjpeg_encoder)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

# Setup OpenMP
find_package(OpenMP REQUIRED)

set(COMMON_COMPILE_FLAGS "-mavx -mfma -ffast-math -m64")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} ${OpenMP_CXX_FLAGS}")

# Setup OpenCL
find_package(OpenCL)
include_directories(${OPENCL_INCLUDE_DIR})

add_executable(
        mjpeg_encoder

        # model
        model/Model.h
        model/Model.cpp

        # io
        io/RawVideoReader.h
        io/RawVideoReader.cpp

        # core
        core/Utils.h
        core/Utils.cpp
        core/AVIOutputStream.h
        core/AVIOutputStream.cpp

        # MJPEG encoder
        core/encoder/AbstractMJPEGEncoder.h
        core/encoder/AbstractMJPEGEncoder.cpp
        core/encoder/MJPEGEncoderSerialImpl.h
        core/encoder/MJPEGEncoderSerialImpl.cpp
        core/encoder/MJPEGEncoderOpenMPImpl.h
        core/encoder/MJPEGEncoderOpenMPImpl.cpp

        # main
        main.cpp
)

# Link libraries

target_link_libraries(
        mjpeg_encoder
        ${OPENCL_LIBRARIES}
        OpenMP::OpenMP_CXX
)
