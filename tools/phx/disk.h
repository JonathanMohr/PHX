#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
    List of dynamic container formats:
        Raw
        VMDK
        VHD(X)
        QCOW2
        VDI

    List of other static container formats:
        E01
*/

typedef struct Disk
{
    uint64_t sectorSize;
} Disk;

#ifdef __cplusplus
}
#endif
