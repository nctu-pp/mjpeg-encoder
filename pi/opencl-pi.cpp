//
// Created by wcl on 2020/11/21.
//

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory.h>
#include <fstream>
#include <vector>
#include <climits>
#include "shishua.h"

#include <CL/cl.h>

int find_devices() {
    // get all platforms
    cl_uint platformCount;

    clGetPlatformIDs(0, nullptr, &platformCount);
    return platformCount; // return number of available devices
}

std::vector<char> get_file_contents(const char *path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size + 1);

    if (!file.read(buffer.data(), size)) {
        fprintf(stderr, "Failed to read file %s", path);
        exit(-1);
    }
    buffer[size] = '\0';

    return buffer;
}

void die_if_error(int line, const cl_int &err) {
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Line %d, OpenCL Error: %d\n", line, err);
        exit(err);
    }
}

void obtain_device(const cl_device_type &type, cl_platform_id &cpPlatform, cl_device_id &device_id) {
    cl_int err;
    // Bind to platform
    err = clGetPlatformIDs(1, &cpPlatform, nullptr);

    die_if_error(__LINE__, err);

    // Get ID for the device
    err = clGetDeviceIDs(cpPlatform, type, 1, &device_id, nullptr);

    die_if_error(__LINE__, err);
}

int main(int argc, char *argv[]) {
    cl_int err;
    uint32_t n = atoll(argv[1]);


    srand(time(0));
    size_t seedLen = sizeof(SEEDTYPE) * 4;
    char tmp[seedLen];
    for (int i = 0, tmpVal; i < seedLen; i += sizeof(int)) {
        tmpVal = rand();
        memcpy(tmp + i, &tmpVal, sizeof(int));
    }
    prng_state s = prng_init((SEEDTYPE *) &tmp);


    // Host input vectors
    double *xArr;
    double *yArr;

    // Host output vector
    int *sumArr;

    // Allocate memory for each vector on host
    xArr = new double[n];
    yArr = new double[n];
    sumArr = new int[n];

    printf("INIT\n");
    int rndTmp[256];
    // Initialize vectors on host

    auto rndTmpPtr = (uint8_t *) rndTmp;
    auto rndTmpLen = (sizeof(rndTmp)) / (sizeof(uint8_t));
    for (int i = 0; i < n; i++) {
        prng_gen(&s, rndTmpPtr, rndTmpLen);
        xArr[i] = (double) rndTmp[0] / INT_MAX;
        yArr[i] = (double) rndTmp[1] / INT_MAX;
    }
    printf("INIT FINISHED\n");

    memset(sumArr, '\0', sizeof(int) * n);

    cl_platform_id cpPlatform; // OpenCL platform
    cl_device_id device_id; // device ID
    obtain_device(CL_DEVICE_TYPE_GPU, cpPlatform, device_id);
    cl_uint maxDimSize = 0;

    err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(maxDimSize), &maxDimSize,
                          nullptr);

    die_if_error(__LINE__, err);

    size_t maxWorkSize;
    err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkSize), &maxWorkSize,
                          nullptr);

    die_if_error(__LINE__, err);

    cl_context context; // context
    cl_command_queue queue; // command queue
    cl_program program; // program
    cl_kernel kernel; // kernel


    // Device input buffers
    cl_mem clX;
    cl_mem clY;

    // Device output buffer
    cl_mem clSum;

    size_t globalSize, localSize;

    // Number of work items in each local work group
    localSize = maxWorkSize / (1 << maxDimSize);

    // Number of total work items - localSize must be devisor
    globalSize = ceil(n / (double) localSize) * localSize;


    // Create a context
    context = clCreateContext(nullptr, 1, &device_id, nullptr, nullptr, &err);
    die_if_error(__LINE__, err);

    // Create a command queue
    queue = clCreateCommandQueue(context, device_id, 0, &err);
    die_if_error(__LINE__, err);

    auto clCodeSrc = get_file_contents("../pi/opencl-pi.cl");
    auto clCodeData = clCodeSrc.data();
    // Create the compute program from the source buffer
    program = clCreateProgramWithSource(
            context, 1, (const char **) &clCodeData, nullptr, &err);

    die_if_error(__LINE__, err);

    // Build the program executable
    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    die_if_error(__LINE__, err);

    // Create the compute kernel in the program we wish to run
    kernel = clCreateKernel(program, "monteCarloInter", &err);
    die_if_error(__LINE__, err);

    // Create the input and output arrays in device memory for our calculation
    size_t int_bytes = sizeof(int) * n;
    size_t double_bytes = sizeof(double) * n;
    clX = clCreateBuffer(context, CL_MEM_READ_ONLY, double_bytes, nullptr, &err);
    clY = clCreateBuffer(context, CL_MEM_READ_ONLY, double_bytes, nullptr, &err);
    clSum = clCreateBuffer(context, CL_MEM_WRITE_ONLY, int_bytes, nullptr, &err);

    // Write our data set into the input array in device memory
    err = clEnqueueWriteBuffer(
            queue, clX, CL_TRUE, 0, double_bytes, xArr, 0, nullptr, nullptr);
    die_if_error(__LINE__, err);
    err = clEnqueueWriteBuffer(
            queue, clY, CL_TRUE, 0, double_bytes, yArr, 0, nullptr, nullptr);
    die_if_error(__LINE__, err);

    // Set the arguments to our compute kernel
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &clX);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &clY);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &clSum);
    err |= clSetKernelArg(kernel, 3, sizeof(uint32_t), &n);
    die_if_error(__LINE__, err);

    // Execute the kernel over the entire range of the data set
    err = clEnqueueNDRangeKernel(
            queue, kernel, 1, nullptr, &globalSize, &localSize, 0, NULL, NULL);

    die_if_error(__LINE__, err);
    // Wait for the command queue to get serviced before reading back results
    clFinish(queue);
    // Read the results from the device
    err = clEnqueueReadBuffer(queue, clSum, CL_TRUE, 0, int_bytes, sumArr, 0, nullptr, nullptr);
    die_if_error(__LINE__, err);

    long sum = 0;
    for (int i = 0; i < n; i++) {
        if (sumArr[i])
            sum++;
    }

    double pi_estimate = 4.0 * sum / ((double) n);

    printf("%lf\n", pi_estimate);

    // release OpenCL resources
    clReleaseMemObject(clSum);
    clReleaseMemObject(clX);
    clReleaseMemObject(clY);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    // release host memory
    delete[] xArr;
    delete[] yArr;
    delete[] sumArr;

    return 0;
}
