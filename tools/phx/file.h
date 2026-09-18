#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <device/device.h>

PHX_Bool PHX_File_Open(const char* path, PHX_Bool readonly, PHX_BlockDevice* out);

#ifdef __cplusplus
}
#endif
