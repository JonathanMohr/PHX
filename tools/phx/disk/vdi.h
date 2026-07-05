#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <disk.h>

Disk_Result VDI_CreateDisk(Disk* disk, const char* path, uint64_t sectorSize, uint64_t sectorCount);
Disk_Result VDI_ReadDisk(Disk* disk, const char* path, bool readOnly);

#ifdef __cplusplus
}
#endif
