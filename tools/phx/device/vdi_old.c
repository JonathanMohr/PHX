#include "vdi.h"
#include "device.h"
#include "file.h"

#include <endianness.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define VDI_IMAGE_SIGNATURE 0xbeda107f

typedef struct VDI_PreHeader
{
    uint8_t comment[64];

    uint32_t signature;

    uint32_t version;
} __attribute__((packed)) VDI_PreHeader;

typedef struct VDI_Data
{
    PHX_File* file;
    
    uint16_t majorVersion;
    uint16_t minorVersion;

    uint8_t comment[65];
} VDI_Data;

static void VDI_Close(Disk* disk)
{
    VDI_Data* data = (VDI_Data*)disk->data;
    PHX_File_Close(data->file);
    free(data);
}

bool VDI_CreateDisk(Disk* disk, const char* path, uint64_t sectorSize, uint64_t sectorCount)
{
    PHX_File* file = PHX_File_Open(path, "w+b");

    disk->close = VDI_Close;

    (void)file;

    (void)sectorSize;
    (void)sectorCount;
    return true;
}

bool VDI_ReadDisk(Disk* disk, const char* path, bool readOnly)
{
    const char* fileMode = readOnly ? "rb" : "r+b";
    PHX_File* file = PHX_File_Open(path, fileMode);
    if (!file)
        return false;

    VDI_Data* data = (VDI_Data*)malloc(sizeof(VDI_Data));
    if (!data)
    {
        PHX_File_Close(file);
        return false;
    }

    VDI_PreHeader preHeader;
    if (PHX_File_Read(file, sizeof(VDI_PreHeader), &preHeader) != sizeof(VDI_PreHeader))
    {
        free(data);
        PHX_File_Close(file);
        return false;
    }

    if (Endian_Convert_u32_Le(preHeader.signature) != 0xbeda107f)
    {
        free(data);
        PHX_File_Close(file);
        return false;
    }

    const uint32_t version = Endian_Convert_u32_Le(preHeader.version);


    data->file = file;

    data->majorVersion = version >> 16;
    data->minorVersion = version & 0xFFFF;

    memcpy(data->comment, preHeader.comment, sizeof(preHeader.comment));
    data->comment[64] = '\0';

    disk->close = VDI_Close;

    disk->data = (void*)data;

    return true;
}
