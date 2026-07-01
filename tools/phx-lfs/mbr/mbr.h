#pragma once

#include <stdint.h>
#include "../disk/disk.h"
#include "../partition/partition.h"
#include "toolchain.h"

#include <toolchain.h>

#define CHUNK_SIZE 16384 /*512*/

#define MBR_TYPE_UNUSED         0x00
#define MBR_TYPE_UNDEFINED      0x7F

#define MBR_TYPE_FAT12          0x01
#define MBR_TYPE_FAT16_L32      0x04
#define MBR_TYPE_FAT16_M32      0x06
#define MBR_TYPE_FAT16_LBA      0x0E
#define MBR_TYPE_FAT32_CHS      0x0B
#define MBR_TYPE_FAT32_LBA      0x0C
#define MBR_TYPE_NTFS_EXFAT     0x07

#define MBR_TYPE_LINUX_SWAP     0x82
#define MBR_TYPE_LINUX_NATIVE   0x83
#define MBR_TYPE_LINUX_LVM      0x8E

#define MBR_TYPE_FREEBSD        0xA5

#define MBR_TYPE_MAC_HFS        0xAB
#define MBR_TYPE_MAC_HFS_BOOT   0xAF

#define MBR_TYPE_GPT_PROTECTIVE 0xEE

#define MBR_TYPE_EFI            0xEF

#define MBR_TYPE_EXTENDED_CHS   0x05
#define MBR_TYPE_EXTENDED_LBA   0x0F

PACKED_BEGIN

typedef struct MBR_Partition {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];

    uint32_t lba_first;
    uint32_t lba_size;

} __attribute__((packed)) MBR_Partition;

typedef struct MBR_Bootsector {
    uint8_t bootcode[446];
    MBR_Partition partitions[4];
    uint16_t signature;

} __attribute__((packed)) MBR_Bootsector;

PACKED_END

typedef struct MBR_Disk {
    Disk* disk;
    MBR_Bootsector bootsector;
} MBR_Disk;

MBR_Disk* MBR_CreateDisk(Disk* disk, int fast, void* bootsector, int force_bootsector);
MBR_Disk* MBR_OpenDisk(Disk* disk);
void MBR_Close(MBR_Disk* mbr, int close_disk);
static inline void MBR_CloseDisk(MBR_Disk* mbr) { MBR_Close(mbr, 1); }

int MBR_DeletePartition(MBR_Disk* mbr, uint8_t index);
int MBR_ClearPartition(MBR_Disk* mbr, uint8_t index);

int MBR_SetPartitionRaw(MBR_Disk* mbr, uint8_t index, uint64_t start, uint64_t size, uint8_t type, int bootable);
Partition* MBR_GetPartitionRaw(MBR_Disk* mbr, uint8_t index, int read_only);

int MBR_WriteBootsector(MBR_Disk* mbr);
int MBR_ReadBootsector(MBR_Disk* mbr);

uint64_t MBR_GetNextFreeRegion(MBR_Disk* mbr, uint64_t start, uint64_t size);
uint64_t MBR_GetEndOfUsedRegion(MBR_Disk* mbr, uint64_t start);

int MBR_PrintAll(MBR_Disk* mbr, const char* name);

static inline uint64_t MBR_GetPartitionStart(const MBR_Partition* partition, uint64_t sectorSize)
{
    return partition->lba_first * sectorSize;
}

static inline uint64_t MBR_GetPartitionSize(const MBR_Partition* partition, uint64_t sectorSize)
{
    return partition->lba_size * sectorSize;
}

static inline uint64_t MBR_GetPartitionEnd(const MBR_Partition* partition, uint64_t sectorSize)
{
    return MBR_GetPartitionStart(partition, sectorSize) + MBR_GetPartitionSize(partition, sectorSize);
}
