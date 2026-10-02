#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <device/device.h>
#include <result.h>

#define PHX_FILE_SIZE_NONE 0
PHX_DetailedResult PHX_File_Open(const char* path, PHX_Bool readonly, PHX_BlockDevice* out, PHX_BlockSize size);

#ifdef __cplusplus
}
#endif
