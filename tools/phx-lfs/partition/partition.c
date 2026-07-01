#include "partition.h"

#include <stdlib.h>

uint64_t Partition_Read(Partition* partition, void* buffer, uint64_t offset, uint64_t size)
{
    if (!buffer || !partition || offset >= partition->size || size == 0) return 0;

    if (offset + size > partition->size) size = partition->size - offset;

    return Disk_Read(partition->disk, buffer, partition->offset + offset, size);
}

uint64_t Partition_Write(Partition* partition, void* buffer, uint64_t offset, uint64_t size)
{
    if (!partition || partition->read_only || !buffer || offset >= partition->size || size == 0) return 0;

    if (offset + size > partition->size) size = partition->size - offset;

    return Disk_Write(partition->disk, buffer, partition->offset + offset, size);
}

Partition* Partition_Create(Disk* disk, uint64_t offset, uint64_t size, int read_only)
{
    if (!disk) return NULL;
    Partition* partition = (Partition*)malloc(sizeof(Partition));
    if (!partition) return NULL;
    partition->disk = disk;
    partition->offset = offset;
    partition->size = size;
    partition->read_only = read_only;
    return partition;
}

void Partition_Close(Partition* partition)
{
    if (!partition) return;
    free(partition);
}
