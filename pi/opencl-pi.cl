// OpenCL kernel. Each work item takes care of one element of c

__kernel void monteCarloInter(__global double* a, __global double* b, __global int* c, const unsigned int n) {
    // Get our global thread ID
    int id = get_global_id(0);

    // Make sure we do not go out of bounds
    if (id < n) {
        double value = a[id] * a[id] + b[id] * b[id];
        if (value <= 1) {
            c[id] = 1;
        } else {
            c[id] = 0;
        }
    }
}
