# MJPEG Video Encoder

Implementation of MJPEG video encoder in OpenMP and OpenCL.

## Requirements
 - GCC 10.0, Clang 11 or above
 - OpenMP 4.5 or above (should included in above compiler)
 - OpenCL 1.2 or above
 - Linux or MinGW

## Build
```
./.build.sh [clean]
```

Use clean if you want to a clean build.

### About configure OpenCL path
If cmake cannot found your OpenCL, you can modify build script, and set following variables

```
-DOpenCL_INCLUDE_DIR=/usr/local/cuda/include/ \
-DOpenCL_LIBRARY=/usr/local/cuda/lib64/libOpenCL.so.1.1
```

## Usage
```
Usage: mjpeg_encoder options
Options:
        -i --input <path>	RAW Video Path.
        -s --size <width>x<height>	Video resolution.
        -r --fps <fps>	Video FPS.
        -o --output <path>	Output path.
        -q --quality <number, [0-100]>	JPEG quality.
        -t --threads <number>	Use number threads.
        -k --kind <serial|openmp|opencl>	Select implementation, default is serial.
        -T --temp-dir <path>	Temp dir, default is system temp dir.
        -d --device <cpu|gpu>	default is cpu.
        -m --measure	Show time measure message.
        -h --help	Show usage.
```
