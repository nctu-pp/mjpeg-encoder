#define _GNU_SOURCE

#include <dlfcn.h>
#include <string.h>

void *dlopen(char const *Fnm, int Flg) {
    void *(*real_dlopen)(char const *, int);
    *(void **) (&real_dlopen) = dlsym(RTLD_NEXT, "dlopen");
    if (Fnm == NULL) {
        return real_dlopen(Fnm, Flg);
    } else if (0 == strcmp("/opt/AMDAPPSDK-3.0/lib/x86_64/sdk/libamdocl64.so", Fnm)) {
        return NULL;
    } else {
        return real_dlopen(Fnm, Flg);
    }
}
