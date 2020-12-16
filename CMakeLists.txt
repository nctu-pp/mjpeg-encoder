cmake_minimum_required(VERSION 3.17)
project(mjpeg_encoder)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

# Setup OpenMP
find_package(OpenMP REQUIRED)

set(COMMON_COMPILE_FLAGS "-mavx -mfma -ffast-math -m64 -mavx2")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize")

    if (CMAKE_BUILD_TYPE STREQUAL Debug)
        set(COMMON_COMPILE_FLAGS "-fno-limit-debug-info")
    endif()
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -march=native")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} ${OpenMP_CXX_FLAGS}")

# Setup OpenCL
find_package(OpenCL)

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
        core/encoder/MJPEGEncoderOpenCLImpl.h
        core/encoder/MJPEGEncoderOpenCLImpl.cpp

        # main
        main.cpp
)

add_executable(
        cl_ping_pong
        cl_ping_pong.cpp
        core/Utils.h
        core/Utils.cpp
)

if (UNIX)
    add_library(hook_amd_opencl SHARED hook/hook-amd-opencl.c)
    add_library(hook_nvidia_opencl SHARED hook/hook-nvidia-opencl.c)
endif()

# Link libraries

if(OpenCL_FOUND)
    message("Found OpenCL: ${OpenCL_VERSION_STRING}, include dirs: ${OpenCL_INCLUDE_DIRS}, lib dirs: ${OpenCL_LIBRARIES}")
    include_directories("${OpenCL_INCLUDE_DIRS}")
    target_link_libraries(
            mjpeg_encoder
            OpenMP::OpenMP_CXX
            OpenCL::OpenCL
    )
    target_link_libraries(
            cl_ping_pong
            OpenCL::OpenCL
    )
else()
    message("OpenCL Not Found")
    target_link_libraries(
            mjpeg_encoder
            OpenMP::OpenMP_CXX
    )
endif()
