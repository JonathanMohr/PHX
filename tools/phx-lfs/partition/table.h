#pragma once

#include <stdint.h>
#include "../disk/disk.h"
#include "../mbr/mbr.h"

typedef unsigned char PartitionTableType;
#define PARTITIONTABLE_NONE  ((PartitionTableType)0)
#define PARTITIONTABLE_MBR   ((PartitionTableType)1)
#define PARTITIONTABLE_GPT   ((PartitionTableType)2)

typedef struct PartitionTable {
    PartitionTableType type;
    Disk* disk;

    union {
        MBR_Disk* mbr;
    } impl;
} PartitionTable;

PartitionTable* PartitionTable_Create(Disk* disk, PartitionTableType type, int fast, void* bootsector, int force_bootsector);
PartitionTable* PartitionTable_Open(Disk* disk, PartitionTableType type);
void PartitionTable_Close(PartitionTable* pt);

PartitionTableType PartitionTable_GetType(PartitionTable* pt);

int PartitionTable_DeletePartition(PartitionTable* pt, uint64_t index);
int PartitionTable_ClearPartition(PartitionTable* pt, uint64_t index);

// TODO
int PartitionTable_SetPartition(PartitionTable* pt, uint64_t index, uint64_t start, uint64_t size, uint8_t type, int bootable);
Partition* PartitionTable_GetPartition(PartitionTable* pt, uint64_t index, int read_only);

uint64_t PartitionTable_GetNextFreeRegion(PartitionTable* pt, uint64_t start, uint64_t size);
uint64_t PartitionTable_GetEndOfUsedRegion(PartitionTable* pt, uint64_t start);

int PartitionTable_PrintAll(PartitionTable* pt, const char* disk_name);
