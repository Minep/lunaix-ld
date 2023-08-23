#ifndef __LD_HELPER
#define __LD_HELPER

#include <stdio.h>
#include <unistd.h>

#define assert(x, msg)                                                         \
    if (!(x)) {                                                                \
        printf("failed: %s", msg);                                             \
        _exit(1);                                                              \
    }

#endif /* __LD_HELPER */
