#include <iostream>
#include <CL/cl.hpp>
#include <vector>
#include <string>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
using namespace std;

struct xorshift128_state {
    uint32_t a, b, c, d;
};

uint32_t xorshift128(struct xorshift128_state *state)
{
	/* Algorithm "xor128" from p. 5 of Marsaglia, "Xorshift RNGs" */
	uint32_t t = state->d;

	uint32_t const s = state->a;
	state->d = state->c;
	state->c = state->b;
	state->b = s;

	t ^= t << 11;
	t ^= t >> 8;
	return state->a = t ^ s ^ (s >> 19);
}

string opencl_kernel = 
R"(
	__kernel void vecadd
	( 
	    __global double *A, 
	    __global double *B,
	    __global double *C,
		__global double *local_result,
		__global long long int *num
	) 
	{
		__private float sum = 0.0f;
		__private int id = get_global_id(0);
		__private int lid = get_local_id(0);

		if (A[id] * A[id] + B[id] * B[id] < 1)
			local_result[id] = 1;
		

		barrier(CLK_LOCAL_MEM_FENCE);

		if (lid == 0) {
			for (long long i = 0; i < num[0]; ++i) {
				C[0] += local_result[i];
			}
			C[0] *= 4;
		}
	}
)";

int main( int argc , char **argv ) {
	
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	if (platforms.empty()) {
		printf("No platforms!\n");		
		return 1;
	}
	
	cl::Platform platform = platforms[0];
	std::vector<cl::Device> Devices;


	platform.getDevices(CL_DEVICE_TYPE_GPU, &Devices);
	if (Devices.empty()) {
		printf("No Devices!\n");
		return 1;
	}

	cl::Device device = Devices[0];
	std::cout << "Device : " << device.getInfo<CL_DEVICE_NAME>() <<"\n";


	cl::Context context({device});
	cl::Program program(context, opencl_kernel);

	if (program.build({device}) != CL_SUCCESS) {
        printf("Fail to build\n");
		cout << program.build({device}) << endl;
        return 1;
    }
	
	long long number_of_toss = 10000;
    unsigned int seed = time(NULL) ^ getpid(); // XOR multiple values to get a semi-unique seed
    xorshift128_state state = {seed, seed, seed, seed};

	std::vector<double> A(number_of_toss);
	std::vector<double> B(number_of_toss);
	std::vector<double> C(number_of_toss, 0);
	std::vector<double> local_result(number_of_toss, 0);
	for (int i = 0; i < number_of_toss; ++i) {
		A[i] = xorshift128(&state)/(double)UINT_MAX;
		B[i] = xorshift128(&state)/(double)UINT_MAX;
	}


	cl::Buffer buffer_A(context, CL_MEM_READ_WRITE, sizeof(double) * number_of_toss); 
	cl::Buffer buffer_B(context, CL_MEM_READ_WRITE, sizeof(double) * number_of_toss);
	cl::Buffer buffer_C(context, CL_MEM_READ_WRITE, sizeof(double) * number_of_toss);
	cl::Buffer buffer_local(context, CL_MEM_READ_WRITE, sizeof(double) * number_of_toss);
	cl::Buffer buffer_num(context, CL_MEM_READ_ONLY, sizeof(long long));

	cl::CommandQueue queue(context, device);

	cl::Kernel vec_add(program, "vecadd");

	vec_add.setArg(0, buffer_A);
	vec_add.setArg(1, buffer_B);
	vec_add.setArg(2, buffer_C);
	vec_add.setArg(3, buffer_local);
	vec_add.setArg(4, buffer_num);

	queue.enqueueWriteBuffer(buffer_A, CL_FALSE, 0, sizeof(double)*number_of_toss, A.data());
    queue.enqueueWriteBuffer(buffer_B, CL_FALSE, 0, sizeof(double)*number_of_toss, B.data());
	queue.enqueueWriteBuffer(buffer_local, CL_FALSE, 0, sizeof(double)*number_of_toss, local_result.data());
	queue.enqueueWriteBuffer(buffer_num, CL_TRUE, 0, sizeof(long long), &number_of_toss);

	queue.enqueueNDRangeKernel(vec_add, cl::NullRange,
                               cl::NDRange(number_of_toss),
                               cl::NullRange);

    queue.enqueueReadBuffer(buffer_C, CL_FALSE, 0, sizeof(double)*number_of_toss, C.data());
	queue.finish();

	cout << "pi: " << C[0]/number_of_toss << endl;

	// for (auto &v : A){
	// 	cout << v << " ";
	// }
	// cout << endl;
	
	// for (auto &v : B){
	// 	cout << v << " ";
	// }
	// cout << endl;
	// for (auto &v : C){
	// 	cout << v << " ";
	// }
	cout << endl;

	return 0;
}