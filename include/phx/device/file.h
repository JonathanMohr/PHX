#ifndef PHX_DEVICE_FILE_H
#define PHX_DEVICE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>

typedef struct PHX_File_Device PHX_File_Device;

typedef struct PHX_File_Operations
{
    PHX_FileSize (*read)(PHX_File_Device* device, void* buffer, PHX_FileSize offset, PHX_FileSize count);
    PHX_FileSize (*write)(PHX_File_Device* device, const void* buffer, PHX_FileSize offset, PHX_FileSize count);

    void (*close)(PHX_File_Device* device);
} PHX_File_Operations;

struct PHX_File_Device
{
    PHX_SectorSize blockSize;
    PHX_FileSize blockCount;

    const PHX_File_Operations* ops;
    void* file;

    PHX_Bool readonly;
};

#ifdef __cplusplus
}
#endif

#endif
