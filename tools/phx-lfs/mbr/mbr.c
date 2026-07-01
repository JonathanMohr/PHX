#include "mbr.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

MBR_Disk* MBR_CreateDisk(Disk* disk, int fast, void* bootsector, int force_bootsector)
{
    if (!disk) return NULL;

    MBR_Disk* mbr = calloc(1, sizeof(MBR_Disk));
    if (!mbr) return NULL;
    mbr->disk = disk;

    if (bootsector) memcpy(&mbr->bootsector, bootsector, sizeof(MBR_Bootsector));

    if (!force_bootsector) {
        memset(mbr->bootsector.partitions, 0, sizeof(mbr->bootsector.partitions));

        mbr->bootsector.signature = 0xAA55;
    }

    if (MBR_WriteBootsector(mbr) != 0) {
        free(mbr);
        return NULL;
    }

    if (!fast) {
        uint64_t offset = 512;
        uint8_t zero_block[CHUNK_SIZE] = {0};
        while (offset < disk->size) {
            uint32_t chunk = (uint32_t)((disk->size - offset) < CHUNK_SIZE ? (disk->size - offset) : CHUNK_SIZE);
            if (Disk_Write(disk, (uint8_t*)zero_block, offset, chunk) != chunk) {
                // TODO
            }
            offset += chunk;
        }
    }

    return mbr;
}

MBR_Disk* MBR_OpenDisk(Disk* disk)
{
    if (!disk) return NULL;

    MBR_Disk* mbr = calloc(1, sizeof(MBR_Disk));
    if (!mbr) return NULL;
    mbr->disk = disk;

    if (MBR_ReadBootsector(mbr) != 0) {
        free(mbr);
        return NULL;
    }

    return mbr;
}

void MBR_Close(MBR_Disk* mbr, int close_disk)
{
    if (!mbr) return;

    if (MBR_WriteBootsector(mbr) != 0) {
        // TODO
    }

    if (close_disk) Disk_Close(mbr->disk);

    free(mbr);
}

int MBR_WriteBootsector(MBR_Disk* mbr)
{
    if (!mbr) return 1;
    uint64_t written = Disk_Write(mbr->disk, &mbr->bootsector, 0, sizeof(MBR_Bootsector));
    if (written != sizeof(MBR_Bootsector)) return 1;
    return 0;
}

int MBR_ReadBootsector(MBR_Disk* mbr)
{
    if (!mbr) return 1;
    uint64_t read = Disk_Read(mbr->disk, &mbr->bootsector, 0, sizeof(MBR_Bootsector));
    if (read != sizeof(MBR_Bootsector)) return 1;
    return 0;
}

static uint64_t FSectorSize;

static int MBR_ComparePartitions(const void* a, const void* b)
{
    const MBR_Partition* pa = (MBR_Partition*)a;
    const MBR_Partition* pb = (MBR_Partition*)b;

    const uint64_t a_start = MBR_GetPartitionStart(pa, FSectorSize);
    const uint64_t b_start = MBR_GetPartitionStart(pb, FSectorSize);

    if (a_start < b_start) return -1;
    if (a_start > b_start) return 1;

    return 0;
}

uint64_t MBR_GetNextFreeRegion(MBR_Disk* mbr, uint64_t start, uint64_t size)
{
    MBR_Partition* partitions = (MBR_Partition*)malloc(4 * sizeof(MBR_Partition));
    if (!partitions) {
        return 0;
    }
    memcpy(partitions, mbr->bootsector.partitions, 4 * sizeof(MBR_Partition));
    FSectorSize = mbr->disk->sectorSize;
    qsort(partitions, 4, sizeof(MBR_Partition), MBR_ComparePartitions);

    uint64_t first_start = MBR_GetPartitionStart(&partitions[0], mbr->disk->sectorSize);
    if (first_start > start && (first_start - start) >= size) {
        free(partitions);
        return start;
    }

    for (uint64_t i = 0; i < 4 - 1; i++) {
        uint64_t end_of_current = MBR_GetPartitionEnd(&partitions[i], mbr->disk->sectorSize);
        uint64_t start_of_next = MBR_GetPartitionStart(&partitions[i+1], mbr->disk->sectorSize);

        if (start_of_next > end_of_current) {
            uint64_t candidate_start = (end_of_current < start) ? start : end_of_current;
            uint64_t gap = start_of_next - candidate_start;
            if (gap >= size) {
                free(partitions);
                return candidate_start;
            }
        }
    }

    uint64_t last_end = MBR_GetPartitionEnd(&partitions[4-1], mbr->disk->sectorSize);
    free(partitions);
    return (last_end < start) ? start : last_end;
}

uint64_t MBR_GetEndOfUsedRegion(MBR_Disk* mbr, uint64_t start)
{
    MBR_Partition* partitions = (MBR_Partition*)malloc(4 * sizeof(MBR_Partition));
    if (!partitions) {
        return 0;
    }
    memcpy(partitions, mbr->bootsector.partitions, 4 * sizeof(MBR_Partition));
    FSectorSize = mbr->disk->sectorSize;
    qsort(partitions, 4, sizeof(MBR_Partition), MBR_ComparePartitions);

    uint64_t last_end = start;

    for (int i = 0; i < 4; i++) {
        //uint64_t part_start = MBR_GetPartitionStart(&partitions[i], mbr->disk->sectorSize);
        uint64_t part_end   = MBR_GetPartitionEnd(&partitions[i], mbr->disk->sectorSize);

        if (part_end > start && part_end > last_end) {
            last_end = part_end;
        }
    }

    free(partitions);
    return last_end;
}

int MBR_PrintAll(MBR_Disk* mbr, const char* name)
{
    if (!mbr || !name) return 1;

    printf("%s:\n", name);

    for (int i = 0; i < 4; i++) {
        const MBR_Partition* partition = &mbr->bootsector.partitions[i];

        printf("- %i:\n", i+1);
        if (partition->type == MBR_TYPE_UNUSED)
            fputs("    Unused\n", stdout);
        else {
            uint64_t start = MBR_GetPartitionStart(partition, mbr->disk->sectorSize);
            uint64_t end = MBR_GetPartitionEnd(partition, mbr->disk->sectorSize);
            uint64_t size = MBR_GetPartitionSize(partition, mbr->disk->sectorSize);

            const uint64_t min = 5;
            const uint64_t precision = 1000;
            char size_suffix = 0;
            uint64_t main = 0;
            uint64_t decimals = 0;

            uint64_t mul = 1024ULL*1024*1024*1024;

            while (mul > 1) {
                decimals = ((size * precision) / mul) % precision;
                main = size / mul;

                while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

                if (size < mul*min && !(main >= 1 && decimals < 10)) {
                    mul /= 1024;
                    continue;
                }

                switch (mul)
                {
                    case 1024ULL*1024*1024*1024: size_suffix = 'T'; break;
                    case 1024ULL*1024*1024: size_suffix = 'G'; break;
                    case 1024ULL*1024: size_suffix = 'M'; break;
                    case 1024ULL: size_suffix = 'K'; break;
                    default: size_suffix = 'B'; break;
                }
                break;
            }

            printf("  - Size: %" PRIu64 " - %" PRIu64 " (%" PRIu64 " bytes", start, end, size);
            if (size_suffix) fprintf(stdout, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
            fputs(")\n", stdout);

            const char* type_undefined = "UNDEFINED";

            const char* type_fat12 = "FAT12";
            const char* type_fat16_chs = "FAT16 CHS";
            const char* type_fat16 = "FAT16";
            const char* type_fat32_chs = "FAT32 CHS";
            const char* type_fat32 = "FAT32";
            const char* type_exfat_ntfs = "NTFS/exFAT";

            const char* type_linux_swap = "Linux Swap";
            const char* type_linux_native = "Linux Native";
            const char* type_linux_lvm = "Linux LVM";

            const char* type_freebsd = "FreeBSD";

            const char* type_mac_hfs = "HFS/HFS+";
            const char* type_mac_hfs_boot = "HFS/HFS+ Boot";

            const char* type_gpt_protective = "GPT Protective";

            const char* type_efi = "EFI";

            const char* type_extended_chs = "Extended CHS";
            const char* type_extended_lba = "Extended LBA";

            const char* type_str = NULL;
            switch (partition->type) {
                case MBR_TYPE_UNDEFINED: type_str = type_undefined; break;

                case MBR_TYPE_FAT12: type_str = type_fat12; break;
                case MBR_TYPE_FAT16_L32: case MBR_TYPE_FAT16_M32: type_str = type_fat16_chs; break;
                case MBR_TYPE_FAT16_LBA: type_str = type_fat16; break;
                case MBR_TYPE_FAT32_CHS: type_str = type_fat32_chs; break;
                case MBR_TYPE_FAT32_LBA: type_str = type_fat32; break;
                case MBR_TYPE_NTFS_EXFAT: type_str = type_exfat_ntfs; break;

                case MBR_TYPE_LINUX_SWAP: type_str = type_linux_swap; break;
                case MBR_TYPE_LINUX_NATIVE: type_str = type_linux_native; break;
                case MBR_TYPE_LINUX_LVM: type_str = type_linux_lvm; break;

                case MBR_TYPE_FREEBSD: type_str = type_freebsd; break;

                case MBR_TYPE_MAC_HFS: type_str = type_mac_hfs; break;
                case MBR_TYPE_MAC_HFS_BOOT: type_str = type_mac_hfs_boot; break;

                case MBR_TYPE_GPT_PROTECTIVE: type_str = type_gpt_protective; break;

                case MBR_TYPE_EFI: type_str = type_efi; break;

                case MBR_TYPE_EXTENDED_CHS: type_str = type_extended_chs; break;
                case MBR_TYPE_EXTENDED_LBA: type_str = type_extended_lba; break;

                default: break;
            }

            printf("  - Type: 0x%" PRIX8 "", partition->type);
            if (type_str) printf(" (%s)", type_str);
            fputc('\n', stdout);

            if (partition->status == 0x80)
                fputs("  - bootable\n", stdout);
            else
                fputs("  - not bootable\n", stdout);

            
        }
    }

    return 0;
}
