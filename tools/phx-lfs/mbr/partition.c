#include "mbr.h"

#include <string.h>

// TODO: better way
#define HEADS 255
#define SECTORS 63
#define MAX_CYLINDER 1023

static void LBA_to_CHS(uint32_t lba, uint8_t chs[3]) {
    uint32_t c, h, s;
    if (lba >= (MAX_CYLINDER + 1) * HEADS * SECTORS) {
        chs[0] = 0xFF;
        chs[1] = 0xFF;
        chs[2] = 0xFF;
        return;
    }

    c = lba / (HEADS * SECTORS);
    uint32_t temp = lba % (HEADS * SECTORS);
    h = temp / SECTORS;
    s = (temp % SECTORS) + 1;

    chs[0] = h & 0xFF;
    chs[1] = ((s & 0x3F) | ((c >> 2) & 0xC0)) & 0xFF;
    chs[2] = c & 0xFF;
}

int MBR_SetPartitionRaw(MBR_Disk* mbr, uint8_t index, uint64_t start, uint64_t size, uint8_t type, int bootable)
{
    if (!mbr || index >= 4) return 1;

    MBR_Partition* partition = &mbr->bootsector.partitions[index];
    partition->status = bootable ? 0x80 : 0x00;

    if (start % mbr->disk->sectorSize != 0 || size % mbr->disk->sectorSize != 0) return 2;

    uint32_t start_lba = (uint32_t)(start / mbr->disk->sectorSize);
    uint32_t size_lba  = (uint32_t)((size + mbr->disk->sectorSize - 1)  / mbr->disk->sectorSize);

    LBA_to_CHS(start_lba, partition->chs_first);
    LBA_to_CHS(start_lba + size_lba - 1, partition->chs_last);

    partition->type = type;

    partition->lba_first = start_lba;
    partition->lba_size = size_lba;

    return 0;
}

Partition* MBR_GetPartitionRaw(MBR_Disk* mbr, uint8_t index, int read_only)
{
    if (!mbr || index >= 4) return NULL;

    MBR_Partition* partition = &mbr->bootsector.partitions[index];

    if (partition->type == MBR_TYPE_UNUSED) return NULL;

    uint64_t start = partition->lba_first * mbr->disk->sectorSize;
    uint64_t size = partition->lba_size * mbr->disk->sectorSize;
    Partition* part = Partition_Create(mbr->disk, start, size, read_only);
    if (!part) return NULL;

    return part;
}

int MBR_DeletePartition(MBR_Disk* mbr, uint8_t index)
{
    if (!mbr || index >= 4) return 1;

    MBR_Partition* partition = &mbr->bootsector.partitions[index];

    if (partition->type == MBR_TYPE_UNUSED) return 2;

    memset(partition, 0, sizeof(MBR_Partition));

    return 0;
}

int MBR_ClearPartition(MBR_Disk* mbr, uint8_t index)
{
    if (!mbr || index >= 4) return 1;

    MBR_Partition* partition = &mbr->bootsector.partitions[index];

    if (partition->type == MBR_TYPE_UNUSED) return 2;

    uint64_t offset = partition->lba_first * mbr->disk->sectorSize;
    uint64_t size = partition->lba_size * mbr->disk->sectorSize;

    uint8_t zero_block[CHUNK_SIZE] = {0};
    uint64_t written = 0;
    while (written < size) {
        uint64_t chunk = (size - written) < CHUNK_SIZE ? (size - written) : CHUNK_SIZE;
        if (Disk_Write(mbr->disk, (uint8_t*)zero_block, offset, chunk) != chunk) {
            //TODO
        }
        offset += chunk;
        written += chunk;
    }

    return 0;
}
