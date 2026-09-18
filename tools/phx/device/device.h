#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

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


typedef struct PHX_BlockDevice
{
    PHX_BlockSize blockSize;
    PHX_BlockSize blockCount;
    void* data;

    PHX_BlockSize (*read)(struct PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count);
    PHX_BlockSize (*write)(struct PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count);

    void (*close)(struct PHX_BlockDevice* device);

    const char* type;

    char name[128];
    PHX_Bool readonly;
} PHX_BlockDevice;


typedef struct PHX_Disk_Interface
{
    PHX_Bool (*getDevice)(PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out);

    const char* type;
    const char* name;
} PHX_Disk_Interface;


PHX_Bool PHX_OpenFile(const char* path, PHX_Bool readonly, PHX_BlockDevice* out);


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
