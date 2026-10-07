#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <device/device.h>
#include <result.h>

typedef enum PHX_File_Mode
{
    PHX_FILE_MODE_READ,
    PHX_FILE_MODE_READ_WRITE,
    PHX_FILE_MODE_CREATE
} PHX_File_Mode;

PHX_DetailedResult PHX_File_Open(const char* path, PHX_File_Mode mode, PHX_BlockDevice* out, PHX_BlockSize size);

#ifdef __cplusplus
}
#endif
