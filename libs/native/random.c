#if defined(_WIN32)
#   define _CRT_RAND_S
#   include <stdlib.h>
#   include <time.h>
#elif defined(__APPLE__)
#   include <stdlib.h>
#elif defined(__linux__)
#   include <sys/random.h>
#   include <time.h>
#else
#   error "Unknown OS"
#endif

#include <phx/native/random.h>

PHX_u32 PHX_Native_GetRandomSeed(void)
{
    PHX_u32 seed;
#if defined(_WIN32)
    unsigned int rs;
    if (rand_s(&rs) == 0)
        seed = rs;
    else
        seed = (PHX_u32)time(NULL);
#elif defined(__APPLE__)
    seed = arc4random();
#elif defined(__linux__)
    if (getrandom(&seed, sizeof(seed), 0) != (ssize_t)sizeof(seed))
        seed = (PHX_u32)time(NULL);
#endif

    return seed;
}
