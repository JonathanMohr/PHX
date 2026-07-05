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

typedef uint8_t Disk_Result;
#define DISK_RESULT_SUCCESS        ((Disk_Result)0)
#define DISK_RESULT_ERROR          ((Disk_Result)1)
#define DISK_RESULT_INVALID_FORMAT ((Disk_Result)2)

typedef struct Disk
{
    /** Returns how many sectors have been read */
    uint64_t (*readSectors)(struct Disk* disk, uint8_t* buffer, uint64_t lba, uint64_t count);
    /** Returns how many sectors have been written */
    uint64_t (*writeSectors)(struct Disk* disk, const uint8_t* buffer, uint64_t lba, uint64_t count);

    uint64_t (*getSectorCount)(struct Disk* disk);
    uint64_t (*getSectorSize)(struct Disk* disk);

    void (*close)(struct Disk* disk);

    void* data;
} Disk;

#ifdef __cplusplus
}
#endif
