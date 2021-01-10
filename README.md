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

Before run OpenCL version, ensure the folder `opencl-kernel` in your working directory.



## Sample Videos

| Filename                | Resolution | FPS  | Download Link                                                |
| ----------------------- | ---------- | ---- | ------------------------------------------------------------ |
| Clip-720x480-f30.raw    | 720x480    | 30   | [Google Drive](https://drive.google.com/file/d/1H-TcgfIhkNfOdvPZBX_DilXZw56Yf30U/view?usp=sharing) |
| winsat-1280x720-f24.raw | 1280x720   | 24   | [Google Drive](https://drive.google.com/file/d/1MtN6grUxq4HKARq9kl_M_r7UtQmP5QEC/view?usp=sharing) |



## Example

- Encode `Clip-720x480-f30.raw` with 3 threads, quality = 95 via OpenMP, save to abc.avi.

  ```
  ./mjpeg_encoder -i Clip-720x480-f30.raw -s 720x480 -r 30 -t 3 -q 95 -k openmp -o abc.avi
  ```

  Output:

  ```
  ...
  Time: 00:00:05.00 / 00:00:05.00
  Video encoded, output file located at abc.avi
  
  RUN TIME: 1118
  ```

  

- Encode `winsat-1280x720-f24.raw` via GPU, save to xyz.avi

  ```
  ./mjpeg_encoder -i winsat-1280x720-f24.raw -s 1280x720 -r 24 -k opencl -d gpu -o xyz.avi
  ```

  Output:

  ```
  Found 1 platforms
  Using device [Intel(R) UHD Graphics 630] on platform [Intel(R) OpenCL HD Graphics].
  Dev Init Time: 1649
  
  PaddingAndTransformColorSpace Time 18
  dctAndQuantization Time 10
  clEncode Time 3524
  GPU TIME: 3562
  DATA READ BACK TIME: 7
  CPU TIME: 6
  
  Time: 00:00:00.54 / 00:00:06.00
  ...
  00:00:06.00 / 00:00:06.00
  
  Video encoded, output file located atxyz.avi
  
  RUN TIME: 39143
  ```



## How to Obtain RAW Format Video

- Install FFmpeg

- Convert your video via:

  `ffmpeg -i input.mp4 -s 854x480 -r 20 -f rawvideo -pix_fmt bgra output.raw`

  854x480 is your video's resolution, 20 is fps, input.mp4 is input video file and output.raw is output file.

