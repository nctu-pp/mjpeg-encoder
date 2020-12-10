//
// Created by wcl on 2020/12/10.
//

#include <iostream>
#include <CL/cl.hpp>
#include <chrono>
#include <sstream>
#include "core/Utils.h"

using namespace std;

void dieIfClError(cl_int err, int line = 0) {
    if (err != CL_SUCCESS) {
        string msg = "OpenCL Error: ";
        msg += to_string(err);

        if (line) {
            msg += ", line: ";
            msg += to_string(line);
        }
        throw runtime_error(msg);
    }
}


int main(int argc, char **argv) {
    vector<cl::Platform> platforms;
    vector<cl::Device> devices;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        throw runtime_error("Cannot found any OpenCL platform");
    }

    cout << "Found " << platforms.size() << " platforms" << endl;

    cl::Device *firstMatchDevice = nullptr;

    cl_int deviceType = CL_DEVICE_TYPE_GPU;
    string platformName;
    for (auto &p : platforms) {
        platformName = p.getInfo<CL_PLATFORM_NAME>(nullptr);
        p.getDevices(deviceType, &devices);

        if (!devices.empty()) {
            firstMatchDevice = new cl::Device(devices[0]);
            break;
        }
    }

    if (!firstMatchDevice) {
        throw runtime_error("Cannot found any OpenCL device.");
    }

    auto _device = firstMatchDevice;

    auto _maxWorkItems = firstMatchDevice->getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
    auto _maxWorkGroupSize = firstMatchDevice->getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    auto _maxMemoryAllocSize = firstMatchDevice->getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    auto _globalMemorySize = firstMatchDevice->getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();

    cout << "Using device ["
         << firstMatchDevice->getInfo<CL_DEVICE_NAME>()
         << "] on platform [" << platformName
         << "]." << endl
         << "Max memory allocation: " << (_maxMemoryAllocSize << 20) << "MB" << endl
         << "Global memory: " << (_globalMemorySize << 20) << "MB" << endl;

    auto _context = new cl::Context({*_device});

    auto _clCmdQueue = new cl::CommandQueue(*_context, *_device);

    stringstream readOut;
    stringstream writeOut;

    /* -------------------------------------------------------------------------------------------
		Loop from 8 B to 1 GB
       --------------------------------------------------------------------------------------------*/

    for (int i = 0; i <= 31; i++) {
        long int N = 1 << i;
        if (N * sizeof(double) > _globalMemorySize) break;

        cerr << N << endl;

        // Allocate memory for A on CPU
        auto totalSize = N * sizeof(double);
        auto *A = (double *) malloc(totalSize);

        cl_int bufferErr = CL_SUCCESS;
        cl::Buffer clBuffer(*_context, CL_MEM_READ_WRITE, totalSize, &bufferErr);
        dieIfClError(bufferErr, __LINE__);

        // Initialize all elements of A to 0.0
        for (int i = 0; i < N; i++) {
            A[i] = 0.0;
        }

        int loop_count = 50;

        long int num_B = 8 * N;
        long int B_in_GB = 1 << 30;
        double num_GB = (double) num_B / (double) B_in_GB;

        char timeBuffer[1000];
        // test write
        do {
            auto &outSS = writeOut;
            // Warm-up loop
            for (int i = 1; i <= 5; i++) {
                _clCmdQueue->enqueueWriteBuffer(
                        clBuffer, CL_TRUE,
                        0, totalSize, A);
            }


            chrono::steady_clock::time_point start_time =
                    chrono::steady_clock::now();

            for (int i = 1; i <= loop_count; i++) {
                _clCmdQueue->enqueueWriteBuffer(
                        clBuffer, CL_TRUE,
                        0, totalSize, A);
            }

            chrono::steady_clock::time_point stop_time =
                    chrono::steady_clock::now();

            auto elapsed_time =
                    (std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time));

            double avg_time_per_transfer = (double) (elapsed_time.count()) / ((double) loop_count);

            sprintf(timeBuffer, "%10li\t%15.30f", num_B, avg_time_per_transfer);

            outSS << timeBuffer << endl;
        } while (false);

        // test read
        do {
            auto &outSS = readOut;
            // Warm-up loop
            for (int i = 1; i <= 5; i++) {
                _clCmdQueue->enqueueReadBuffer(
                        clBuffer, CL_TRUE,
                        0, totalSize, A);
            }


            chrono::steady_clock::time_point start_time =
                    chrono::steady_clock::now();

            for (int i = 1; i <= loop_count; i++) {
                _clCmdQueue->enqueueReadBuffer(
                        clBuffer, CL_TRUE,
                        0, totalSize, A);
            }

            chrono::steady_clock::time_point stop_time =
                    chrono::steady_clock::now();

            auto elapsed_time =
                    (std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time));
            double avg_time_per_transfer = (double) (elapsed_time.count()) / ((double) loop_count);

            sprintf(timeBuffer, "%10li\t%15.30f", num_B, avg_time_per_transfer);

            outSS << timeBuffer << endl;
        } while (false);

        free(A);
    }

    cout << "================================" << endl
         << "Write" << endl
         << writeOut.str() << endl
         << "================================" << endl
         << "Read" << endl
         << readOut.str() << endl;


    _clCmdQueue->finish();

    delete _clCmdQueue;
    delete _context;
    delete _device;

    return 0;
}
