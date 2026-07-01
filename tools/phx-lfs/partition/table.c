#include "table.h"

#include "partition.h"

#include <stdlib.h>

PartitionTable* PartitionTable_Create(Disk* disk, PartitionTableType type, int fast, void* bootsector, int force_bootsector)
{
    PartitionTable* pt = (PartitionTable*)malloc(sizeof(PartitionTable));
    if (!pt) return NULL;

    pt->type = type;
    pt->disk = disk;

    pt->impl.mbr = MBR_CreateDisk(disk, fast, bootsector, force_bootsector);
    if (!pt->impl.mbr)
    {
        free(pt);
        return NULL;
    }

    return pt;
}

PartitionTable* PartitionTable_Open(Disk* disk, PartitionTableType type)
{
    PartitionTable* pt = (PartitionTable*)malloc(sizeof(PartitionTable));
    if (!pt) return NULL;

    pt->type = type;
    pt->disk = disk;

    pt->impl.mbr = MBR_OpenDisk(disk);
    if (!pt->impl.mbr)
    {
        free(pt);
        return NULL;
    }

    return pt;
}

void PartitionTable_Close(PartitionTable* pt)
{
    MBR_Close(pt->impl.mbr, 0);
    free(pt);
}

PartitionTableType PartitionTable_GetType(PartitionTable* pt)
{
    return pt->type;
}

int PartitionTable_DeletePartition(PartitionTable* pt, uint64_t index)
{
    if (!pt) return 1;
    return MBR_DeletePartition(pt->impl.mbr, (uint8_t)index);
}

int PartitionTable_ClearPartition(PartitionTable* pt, uint64_t index)
{
    if (!pt) return 1;
    return MBR_ClearPartition(pt->impl.mbr, (uint8_t)index);
}

// TODO
int PartitionTable_SetPartition(PartitionTable* pt, uint64_t index, uint64_t start, uint64_t size, uint8_t type, int bootable)
{
    if (!pt) return 1;
    return MBR_SetPartitionRaw(pt->impl.mbr, (uint8_t)index, start, size, type, bootable);
}

Partition* PartitionTable_GetPartition(PartitionTable* pt, uint64_t index, int read_only)
{
    if (!pt) return NULL;
    return MBR_GetPartitionRaw(pt->impl.mbr, (uint8_t)index, read_only);
}

uint64_t PartitionTable_GetNextFreeRegion(PartitionTable* pt, uint64_t start, uint64_t size)
{
    return MBR_GetNextFreeRegion(pt->impl.mbr, start, size);
}

uint64_t PartitionTable_GetEndOfUsedRegion(PartitionTable* pt, uint64_t start)
{
    return MBR_GetEndOfUsedRegion(pt->impl.mbr, start);
}

int PartitionTable_PrintAll(PartitionTable* pt, const char* disk_name)
{
    return MBR_PrintAll(pt->impl.mbr, disk_name);
}
