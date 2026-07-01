#include "fat.h"
#include <inttypes.h>

void FAT_DumpInfo(FAT_Filesystem* fs, FILE* s, int count_fat)
{
    switch (fs->version)
    {
        case FAT_VERSION_12: fputs("FAT12 Filesystem:\n", s); break;
        case FAT_VERSION_16: fputs("FAT16 Filesystem:\n", s); break;
        case FAT_VERSION_32: fputs("FAT32 Filesystem:\n", s); break;
        default: fputs("Unknown Filesystem:\n", s); break;
    }

    const uint64_t min = 5;
    const uint64_t precision = 1000;

    if (fs->version == FAT_VERSION_32) {
        FAT32_Bootsector_Header* header = &fs->bootsector.fat32.header;

        fprintf(s, " OEM-Name: '%.8s'\n", header->oem_name);
        fprintf(s, " Volume Label: '%.11s'\n", header->volume_label);
        fprintf(s, " Volume ID: 0x%" PRIx32 "\n", header->volume_id);

        fprintf(s, " Bytes per sector: %" PRIu16 "\n", header->bytes_per_sector);
        fprintf(s, " Sectors per cluster: %" PRIu8 " (%" PRIu64 " bytes)\n", header->sectors_per_cluster, fs->cluster_size);
        fprintf(s, " Reserved sectors: %" PRIu16 "\n", header->reserved_sectors);

        fprintf(s, " Number of FATs: %" PRIu8 "\n", header->number_of_fats);
        // No max root directory entries as it's FAT32
        
        uint64_t total_sectors = (uint64_t)(header->total_sectors_large ? header->total_sectors_large : header->total_sectors_small);
        uint64_t total_bytes = (uint64_t)total_sectors*header->bytes_per_sector;

        char size_suffix = 0;
        uint64_t main = 0;
        uint64_t decimals = 0;

        uint64_t mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((total_bytes * precision) / mul) % precision;
            main = total_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (total_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Total sectors: %" PRIu64 " (%" PRIu64 " bytes", total_sectors, total_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);

        const char* media_descriptor_name_floppy = "1.44MB Floppy";
        const char* media_descriptor_name_disk = "Disk";
        const char* media_descriptor_name = NULL;
        switch (header->media_descriptor)
        {
            case FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY144:
                media_descriptor_name = media_descriptor_name_floppy;
                break;
            case FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_DISK:
                media_descriptor_name = media_descriptor_name_disk;
                break;
            default:
                break;
        }

        fprintf(s, " Media Descriptor: 0x%" PRIx8, header->media_descriptor);
        if (media_descriptor_name) fprintf(s, " (%s)", media_descriptor_name);
        fputc('\n', s);

        uint64_t fat_sectors = (header->fat_size_32 ? header->fat_size_32 : header->fat_size_small);
        total_bytes = (uint64_t)fat_sectors*header->bytes_per_sector;

        size_suffix = 0;
        main = 0;
        decimals = 0;

        mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((total_bytes * precision) / mul) % precision;
            main = total_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (total_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Fat size: %" PRIu64 " Sectors (%" PRIu64 " bytes", fat_sectors, total_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);

        fprintf(s, " Sectors per track: %" PRIu16 "\n", header->sectors_per_track);
        fprintf(s, " Number of heads: %" PRIu16 "\n", header->number_of_heads);

        fprintf(s, " Drive Number: 0x%" PRIu8 "\n", header->drive_number);

        // FAT32 only
        fprintf(s, " FS Version: %" PRIu16 "\n", header->fs_version);
        fprintf(s, " Ext Flags: 0x%" PRIx16 "\n", header->ext_flags);
    } else {
        FAT12_FAT16_Bootsector_Header* header = &fs->bootsector.fat12_fat16.header;

        fprintf(s, " OEM-Name: '%.8s'\n", header->oem_name);
        fprintf(s, " Volume Label: '%.11s'\n", header->volume_label);
        fprintf(s, " Volume ID: 0x%" PRIx32 "\n", header->volume_id);

        fprintf(s, " Bytes per sector: %" PRIu16 "\n", header->bytes_per_sector);
        fprintf(s, " Sectors per cluster: %" PRIu8 " (%" PRIu64 " bytes)\n", header->sectors_per_cluster, fs->cluster_size);
        fprintf(s, " Reserved sectors: %" PRIu16 "\n", header->reserved_sectors);

        fprintf(s, " Number of FATs: %" PRIu8 "\n", header->number_of_fats);
        fprintf(s, " Max root directory entries: %" PRIu16 "\n", header->max_root_directory_entries);

        uint64_t total_sectors = (uint64_t)(header->large_total_sectors ? header->large_total_sectors : header->total_sectors);
        uint64_t total_bytes = (uint64_t)total_sectors*header->bytes_per_sector;

        char size_suffix = 0;
        uint64_t main = 0;
        uint64_t decimals = 0;

        uint64_t mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((total_bytes * precision) / mul) % precision;
            main = total_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (total_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Total sectors: %" PRIu64 " (%" PRIu64 " bytes", total_sectors, total_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);

        const char* media_descriptor_name_floppy = "1.44MB Floppy";
        const char* media_descriptor_name_disk = "Disk";
        const char* media_descriptor_name = NULL;
        switch (header->media_descriptor)
        {
            case FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY144:
                media_descriptor_name = media_descriptor_name_floppy;
                break;
            case FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_DISK:
                media_descriptor_name = media_descriptor_name_disk;
                break;
            default:
                break;
        }

        fprintf(s, " Media Descriptor: 0x%" PRIx8, header->media_descriptor);
        if (media_descriptor_name) fprintf(s, " (%s)", media_descriptor_name);
        fputc('\n', s);

        uint64_t fat_sectors = (uint64_t)header->fat_size;
        total_bytes = (uint64_t)fat_sectors*header->bytes_per_sector;

        size_suffix = 0;
        main = 0;
        decimals = 0;

        mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((total_bytes * precision) / mul) % precision;
            main = total_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (total_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Fat size: %" PRIu64 " Sectors (%" PRIu64 " bytes", fat_sectors, total_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);

        fprintf(s, " Sectors per track: %" PRIu16 "\n", header->sectors_per_track);
        fprintf(s, " Number of heads: %" PRIu16 "\n", header->number_of_heads);

        fprintf(s, " Drive Number: 0x%" PRIu8 "\n", header->drive_number);
    }

    if (fs->version == FAT_VERSION_32) {
        uint64_t free_bytes = fs->fs_info.free_cluster_count * fs->cluster_size;

        char size_suffix = 0;
        uint64_t main = 0;
        uint64_t decimals = 0;

        uint64_t mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((free_bytes * precision) / mul) % precision;
            main = free_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (free_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Free Cluster Count: %" PRIu32, fs->fs_info.free_cluster_count);
        fprintf(s, " (%" PRIu64 " bytes", free_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);
    } else if (count_fat) {
        uint32_t free_clusters = 0;
        uint32_t current_cluster = 2; //first 2 are reserved
        uint32_t total_clusters = (uint32_t)(fs->data_size / fs->cluster_size);
        while (current_cluster < total_clusters) {
            uint32_t value = FAT_ReadFATEntry(fs, current_cluster);
            if (FAT_ClusterType(fs, value) == FAT_CLUSTER_FREE) free_clusters++;
            current_cluster++;
        }

        uint64_t free_bytes = free_clusters * fs->cluster_size;

        char size_suffix = 0;
        uint64_t main = 0;
        uint64_t decimals = 0;

        uint64_t mul = 1024ULL*1024*1024*1024;

        while (mul > 1) {
            decimals = ((free_bytes * precision) / mul) % precision;
            main = free_bytes / mul;

            while (decimals != 0 && decimals % 10 == 0) decimals /= 10;

            if (free_bytes < mul*min && !(main >= 1 && decimals < 10)) {
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

        fprintf(s, " Free Cluster Count: %" PRIu32, free_clusters);
        fprintf(s, " (%" PRIu64 " bytes", free_bytes);
        if (size_suffix) fprintf(s, " ~ %" PRIu64 ".%" PRIu64 "%c", main, decimals, size_suffix);
        fputs(")\n", s);
    } else {
        fputs(" Free Cluster Count: Not counted\n", s);
    }
}
