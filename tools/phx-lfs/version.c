#include "version.h"

#include <stdio.h>

#ifndef VERSION
#error "VERSION not defined"
#endif

void printVersion(void)
{
    fputs("PHX-LFS version " VERSION "\n", stdout);
    fputs("Compiled on " __DATE__ "\n", stdout);
    fputs("License: Apache 2.0\n", stdout);
    fflush(stdout);
}
