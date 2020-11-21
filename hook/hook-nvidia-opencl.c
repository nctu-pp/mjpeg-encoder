#define _GNU_SOURCE

#include <dlfcn.h>
#include <string.h>

void *dlopen(char const *Fnm, int Flg) {
    void *(*real_dlopen)(char const *, int);
    *(void **) (&real_dlopen) = dlsym(RTLD_NEXT, "dlopen");

    if (Fnm == NULL) {
        return real_dlopen(Fnm, Flg);
    } else if (0 == strcmp("/usr/lib/x86_64-linux-gnu/libnvidia-opencl.so.1", Fnm)) {
        return NULL;
    } else {
        return real_dlopen(Fnm, Flg);
    }
}
