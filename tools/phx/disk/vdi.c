#include "vdi.h"
#include "disk.h"
#include "file.h"

#include <stdio.h>
#include <stdint.h>

#define VDI_IMAGE_SIGNATURE 0xbeda107f

typedef struct VDI_PreHeader
{
    uint8_t comment[64];

    uint32_t signature;

    uint32_t version;
} __attribute__((packed)) VDI_PreHeader;

typedef struct VDI_Data
{
    uint8_t comment[64];

    uint16_t majorVersion;
    uint16_t minorVersion;

} VDI_Data;

Disk_Result VDI_CreateDisk(Disk* disk, const char* path, uint64_t sectorSize, uint64_t sectorCount)
{
    (void)disk;
    (void)path;
    (void)sectorSize;
    (void)sectorCount;
    return DISK_RESULT_SUCCESS;
}

Disk_Result VDI_ReadDisk(Disk* disk, const char* path, bool readOnly)
{
    PHX_File* file = PHX_File_Open(path, "rb");

    VDI_PreHeader preHeader;

    (void)file;
    (void)preHeader;

    (void)disk;
    (void)readOnly;
    return DISK_RESULT_SUCCESS;
}
