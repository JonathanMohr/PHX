#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "device.h"

bool VDI_CreateDisk(Disk* disk, const char* path, uint64_t sectorSize, uint64_t sectorCount);
bool VDI_ReadDisk(Disk* disk, const char* path, bool readOnly);

#ifdef __cplusplus
}
#endif
