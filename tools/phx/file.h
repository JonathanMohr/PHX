#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <result.h>

typedef struct PHX_FileDevice
{
    PHX_FileSize fileSize;
    void* file;
    PHX_Bool readonly;
} PHX_FileDevice;

typedef enum PHX_File_Mode
{
    PHX_FILE_MODE_READ,
    PHX_FILE_MODE_READ_WRITE,
    PHX_FILE_MODE_CREATE
} PHX_File_Mode;

PHX_DetailedResult PHX_File_Open(const char* path, PHX_File_Mode mode, PHX_FileDevice* out, PHX_FileSize size);
void PHX_File_Close(PHX_FileDevice* device);

PHX_FileSize PHX_File_Write(PHX_FileDevice* device, const void* buffer, PHX_FileSize offset, PHX_FileSize count);
PHX_FileSize PHX_File_Read(PHX_FileDevice* device, void* buffer, PHX_FileSize offset, PHX_FileSize count);

#ifdef __cplusplus
}
#endif
