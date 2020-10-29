cmake_minimum_required(VERSION 3.17)
project(mjpeg_encoder)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

# Setup OpenMP
find_package(OpenMP REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

# Setup OpenCL
find_package(OpenCL)
include_directories(${OPENCL_INCLUDE_DIR})


add_executable(mjpeg_encoder main.cpp)

# Link libraries
target_link_libraries(mjpeg_encoder
        ${OPENCL_LIBRARIES}
        OpenMP::OpenMP_CXX
)
