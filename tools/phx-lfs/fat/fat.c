#include "fat.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

static int FAT_IsInvalidChar(char c) {
    return (c < 0x20) || c == '"' || c == '*' || c == '/' || c == ':' ||
           c == '<' || c == '>' || c == '?' || c == '\\' || c == '|' ||
           c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']';
}

int FAT_ParseName(const char* name, char fat_name[8], char fat_ext[3])
{
    fat_name[0] = ' '; fat_name[1] = ' ';
    fat_name[2] = ' '; fat_name[3] = ' ';
    fat_name[4] = ' '; fat_name[5] = ' ';
    fat_name[6] = ' '; fat_name[7] = ' ';
    fat_ext[0] = ' '; fat_ext[1] = ' '; fat_ext[2] = ' ';

    size_t len = strlen(name);

    const char* dot = strrchr(name, '.');
    size_t nlen = dot ? (size_t)(dot - name) : len;
    if (nlen > 8) nlen = 8;

    for (size_t i = 0; i < nlen; i++) {
        char c = (char)toupper((int)(unsigned char)name[i]);
        if (FAT_IsInvalidChar(c)) c = '_';
        fat_name[i] = c;
    }

    if (dot) {
        size_t elen = len - (size_t)(dot - name) - 1;
        if (elen > 3) elen = 3;
        for (size_t i = 0; i < elen; i++) {
            char c = (char)toupper((int)(unsigned char)dot[i + 1]);
            if (FAT_IsInvalidChar(c)) c = '_';
            fat_ext[i] = c;
        }
    }

    return 0;
}

int FAT_FlushFATBuffer(FAT_Filesystem* fs)
{
    if (!fs) return 1;
    if (fs->read_only) return 0;
    if (Partition_Write(fs->partition, (uint8_t*)fs->fat_buffer, fs->fat_offset + fs->fat_buffer_start, FAT_BUFFER_SIZE) != FAT_BUFFER_SIZE) return 1;
    return 0;
}

int FAT_LoadFATBuffer(FAT_Filesystem* fs, uint64_t offset)
{
    if (!fs) return 1;
    if (Partition_Read(fs->partition, (uint8_t*)fs->fat_buffer, fs->fat_offset + offset, FAT_BUFFER_SIZE) != FAT_BUFFER_SIZE) return 1;
    fs->fat_buffer_start = offset;
    return 0;
}

FAT_Filesystem* FAT_CreateEmptyFilesystem(Partition* partition, Fat_Version version, int fast, void* bootsector, int force_bootsector,
                                          const char* oem_name, const char* volume_label, uint32_t volume_id,
                                          uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                                          uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                                          uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                                          uint8_t media_descriptor, uint32_t hidden_sectors)
{
    if (!partition || !oem_name || !volume_label) return NULL;
    if (version != FAT_VERSION_12 && version != FAT_VERSION_16 && version != FAT_VERSION_32) return NULL;
    FAT_Filesystem* fs = calloc(1, sizeof(FAT_Filesystem));
    fs->partition = partition;
    fs->version = version;
    fs->read_only = 0;

    if (fs->version == FAT_VERSION_12 || fs->version == FAT_VERSION_16) {
        if (FAT12_FAT16_WriteBootsector(fs, bootsector, force_bootsector, oem_name, volume_label, volume_id, total_size, bytes_per_sector, sectors_per_cluster,
                                        reserved_sectors, number_of_fats, max_root_directory_entries, sectors_per_track,
                                        number_of_heads, drive_number, media_descriptor, hidden_sectors) != 0) {
            free(fs);
            return NULL;
        }

        fs->size = total_size;

        fs->fat_offset = (uint64_t)fs->bootsector.fat12_fat16.header.reserved_sectors * fs->bootsector.fat12_fat16.header.bytes_per_sector;
        fs->fat_size = (uint64_t)fs->bootsector.fat12_fat16.header.fat_size * fs->bootsector.fat12_fat16.header.bytes_per_sector;

        fs->root_offset = fs->fat_offset + fs->fat_size * fs->bootsector.fat12_fat16.header.number_of_fats;
        fs->root_size = fs->bootsector.fat12_fat16.header.max_root_directory_entries * sizeof(FAT_DirectoryEntry);

        fs->data_offset = fs->root_offset + fs->root_size;
        fs->data_size = fs->size - fs->data_offset;

        fs->cluster_size = (uint64_t)fs->bootsector.fat12_fat16.header.bytes_per_sector * fs->bootsector.fat12_fat16.header.sectors_per_cluster;
    } else {
        if (FAT32_WriteBootsector(fs, bootsector, force_bootsector, oem_name, volume_label, volume_id, total_size, bytes_per_sector, sectors_per_cluster,
                                  reserved_sectors, number_of_fats, max_root_directory_entries, sectors_per_track,
                                  number_of_heads, drive_number, media_descriptor, hidden_sectors) != 0) {
            free(fs);
            return NULL;
        }

        fs->size = total_size;

        fs->fat_offset = (uint64_t)fs->bootsector.fat32.header.reserved_sectors * fs->bootsector.fat32.header.bytes_per_sector;
        fs->fat_size = (uint64_t)fs->bootsector.fat32.header.fat_size_32 * fs->bootsector.fat32.header.bytes_per_sector;

        fs->data_offset = fs->fat_offset + fs->fat_size * fs->bootsector.fat32.header.number_of_fats;
        fs->data_size = fs->size - fs->data_offset;

        fs->root_offset = fs->data_offset + (fs->bootsector.fat32.header.root_cluster - 2) * fs->cluster_size;
        fs->root_size = 0;

        fs->cluster_size = (uint64_t)fs->bootsector.fat32.header.bytes_per_sector * fs->bootsector.fat32.header.sectors_per_cluster;
    }

    if (FAT_WriteEmptyFAT(fs) != 0) {
        free(fs);
        return NULL;
    }
    for (uint8_t i = 1; i < number_of_fats; i++) {
        if (FAT_CopyFAT(fs, i, 0) != 0) {
            free(fs);
            return NULL;
        }
    }

    if (FAT_WriteEmptyRootDir(fs) != 0) {
        free(fs);
        return NULL;
    }

    if (!fast) {
        // fill data area
        uint64_t offset = fs->data_offset;
        uint8_t zero_block[CHUNK_SIZE] = {0};
        uint64_t written = 0;
        while (written < fs->data_size) {
            uint64_t chunk = (fs->data_size - written) < CHUNK_SIZE ? (fs->data_size - written) : CHUNK_SIZE;
            if (Partition_Write(fs->partition, (uint8_t*)zero_block, offset, chunk) != chunk) {
                free(fs);
                return NULL;
            }
            offset += chunk;
            written += chunk;
        }
    }

    if (FAT_LoadFATBuffer(fs, 0) != 0) {
        free(fs);
        return NULL;
    }

    if (fs->version == FAT_VERSION_12 || fs->version == FAT_VERSION_16) {
        fs->static_root.fs = fs;
        fs->static_root.size = fs->root_size;
        fs->static_root.first_cluster = 0;
        fs->static_root.directory_entry_offset = 0;
        fs->static_root.is_root_directory = 1;
        fs->static_root.is_directory = 1;
        fs->static_root.is_root_directory_fat32 = 0;
    } else {
        fs->static_root.fs = fs;
        fs->static_root.size = fs->root_size;
        fs->static_root.first_cluster = 0;
        fs->static_root.directory_entry_offset = 0;
        fs->static_root.is_root_directory = 0;
        fs->static_root.is_directory = 1;
        fs->static_root.is_root_directory_fat32 = 1;

        if (FAT_ReserveSpace(&fs->static_root, fs->cluster_size, 0) != 0) {
            free(fs);
            return NULL;
        }
    }

    fs->root = &fs->static_root;

    return fs;
}

FAT_Filesystem* FAT_OpenFilesystem(Partition* partition, Fat_Version version, int read_only)
{
    if (!partition) return NULL;
    if (version != FAT_VERSION_12 && version != FAT_VERSION_16 && version != FAT_VERSION_32) return NULL;
    FAT_Filesystem* fs = calloc(1, sizeof(FAT_Filesystem));
    fs->partition = partition;
    fs->version = version;
    fs->read_only = read_only;
    
    if (Partition_Read(partition, (uint8_t*)fs->bootsector.buffer, 0, sizeof(FAT_Bootsector)) != sizeof(FAT_Bootsector)) {
        free(fs);
        return NULL;
    }

    if (fs->version == FAT_VERSION_12 || fs->version == FAT_VERSION_16) {
        FAT12_FAT16_Bootsector_Header* header = &fs->bootsector.fat12_fat16.header;

        uint32_t total_sectors;
        if (header->total_sectors == 0)
            total_sectors = header->large_total_sectors;
        else
            total_sectors = (uint32_t)header->total_sectors;

        fs->size = (uint64_t)total_sectors * header->bytes_per_sector;

        fs->fat_offset = (uint64_t)header->reserved_sectors * header->bytes_per_sector;
        fs->fat_size = (uint64_t)header->fat_size * header->bytes_per_sector;

        fs->root_offset = fs->fat_offset + fs->fat_size * header->number_of_fats;
        fs->root_size = header->max_root_directory_entries * sizeof(FAT_DirectoryEntry);

        fs->data_offset = fs->root_offset + fs->root_size;
        fs->data_size = fs->size - fs->data_offset;

        fs->cluster_size = (uint64_t)header->bytes_per_sector * header->sectors_per_cluster;
    } else {
        FAT32_Bootsector_Header* header = &fs->bootsector.fat32.header;
        
        uint32_t total_sectors = header->total_sectors_large;

        fs->size = (uint64_t)total_sectors * header->bytes_per_sector;

        fs->fat_offset = (uint64_t)header->reserved_sectors * header->bytes_per_sector;
        fs->fat_size = (uint64_t)header->fat_size_32 * header->bytes_per_sector;

        fs->data_offset = fs->fat_offset + fs->fat_size * header->number_of_fats;
        fs->data_size = fs->size - fs->data_offset;

        fs->root_offset = fs->data_offset + (header->root_cluster - 2) * fs->cluster_size;
        fs->root_size = 0;

        fs->cluster_size = (uint64_t)header->bytes_per_sector * header->sectors_per_cluster;

        uint64_t fs_info_offset = (uint64_t)header->fs_info_sector * header->bytes_per_sector;

        if (Partition_Read(partition, (uint8_t*)&fs->fs_info, fs_info_offset, sizeof(FAT32_FS_Info)) != sizeof(FAT32_FS_Info)) {
            free(fs);
            return NULL;
        }
    }

    if (FAT_LoadFATBuffer(fs, 0) != 0) {
        free(fs);
        return NULL;
    }

    if (fs->version == FAT_VERSION_12 || fs->version == FAT_VERSION_16) {
        fs->static_root.fs = fs;
        fs->static_root.size = fs->root_size;
        fs->static_root.first_cluster = 0;
        fs->static_root.directory_entry_offset = 0;
        fs->static_root.is_root_directory = 1;
        fs->static_root.is_directory = 1;
        fs->static_root.is_root_directory_fat32 = 0;
    } else {
        fs->static_root.fs = fs;
        fs->static_root.first_cluster = fs->bootsector.fat32.header.root_cluster;
        fs->static_root.directory_entry_offset = 0;
        fs->static_root.is_root_directory = 0;
        fs->static_root.is_directory = 1;
        fs->static_root.is_root_directory_fat32 = 1;

        uint32_t cluster_count = 0;
        uint32_t cluster = fs->static_root.first_cluster;
        while (FAT_ClusterType(fs, cluster) != FAT_CLUSTER_EOC) {
            cluster_count++;
            cluster = FAT_ReadFATEntry(fs, cluster);
        }
        fs->static_root.size = fs->cluster_size * cluster_count;
    }

    fs->root = &fs->static_root;

    return fs;
}

void FAT_CloseFilesystem(FAT_Filesystem* fs)
{
    if (!fs->read_only) {
        if (FAT_WriteBootsector(fs) != 0) {
            //TODO
        }

        if (fs->version == FAT_VERSION_32) {
            if (FAT32_WriteFSInfo(fs) != 0) {
                //TODO
            }
        }

        if (FAT_FlushFATBuffer(fs) != 0) {
            //TODO
        }

        uint8_t number_of_fats = fs->version == FAT_VERSION_32 ? fs->bootsector.fat32.header.number_of_fats : fs->bootsector.fat12_fat16.header.number_of_fats;
        for (uint8_t i = 1; i < number_of_fats; i++) {
            if (FAT_CopyFAT(fs, i, 0) != 0) {
                //TODO
            }
        }
    }

    free(fs);
}


uint32_t FAT_ReadFATEntry(FAT_Filesystem* fs, uint32_t cluster)
{
    if (!fs || cluster > FAT_GetLastCluster(fs)) return 0xFFFFFFFF;

    uint64_t offset;
    uint8_t* bytes;
    uint64_t rel;

    switch (fs->version)
    {
        case FAT_VERSION_12:
            offset = ((uint64_t)cluster * 3) / 2;

            if (offset < fs->fat_buffer_start ||
                offset + 1 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 0xFFFFFFFF;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 0xFFFFFFFF;
            }

            rel   = offset - fs->fat_buffer_start;
            bytes = &fs->fat_buffer[rel];

            if (cluster & 1)
                return ((bytes[0] >> 4) | (bytes[1] << 4)) & 0x0FFF;
            else
                return  (bytes[0] | ((bytes[1] & 0x0F) << 8)) & 0x0FFF;


        case FAT_VERSION_16:
            offset = (uint64_t)cluster * 2;

            if (offset < fs->fat_buffer_start ||
                offset + 1 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 0xFFFFFFFF;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 0xFFFFFFFF;
            }

            rel   = offset - fs->fat_buffer_start;
            bytes = &fs->fat_buffer[rel];

            return (uint32_t)(bytes[0] | (bytes[1] << 8));


        case FAT_VERSION_32:
            offset = (uint64_t)cluster * 4;

            if (offset < fs->fat_buffer_start ||
                offset + 3 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 0xFFFFFFFF;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 0xFFFFFFFF;
            }

            rel   = offset - fs->fat_buffer_start;
            bytes = &fs->fat_buffer[rel];

            return (bytes[0] |
                   (bytes[1] << 8) |
                   (bytes[2] << 16) |
                   (bytes[3] << 24)) & 0x0FFFFFFF;

        default: break;
    }

    return 0xFFFFFFFF;
}

int FAT_WriteFATEntry(FAT_Filesystem* fs, uint32_t cluster, uint32_t value)
{
    if (!fs || fs->read_only || cluster > FAT_GetLastCluster(fs)) return 1;

    uint64_t offset;
    uint8_t* entry_bytes;
    uint64_t rel;

    switch (fs->version)
    {
        case FAT_VERSION_12:
            value &= 0x0FFF;
            offset = ((uint64_t)cluster * 3) / 2;

            if (offset < fs->fat_buffer_start ||
                offset + 1 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 1;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 1;
            }

            rel = offset - fs->fat_buffer_start;
            entry_bytes = &fs->fat_buffer[rel];

            if (cluster & 1)
            {
                entry_bytes[0] = (uint8_t)((entry_bytes[0] & 0x0F) | ((value & 0x0F) << 4));
                entry_bytes[1] = (value >> 4) & 0xFF;
            }
            else
            {
                entry_bytes[0] = (value & 0xFF);
                entry_bytes[1] = (uint8_t)((entry_bytes[1] & 0xF0) | ((value >> 8) & 0x0F));
            }
            return 0;


        case FAT_VERSION_16:
            value &= 0xFFFF;
            offset = (uint64_t)cluster * 2;

            if (offset < fs->fat_buffer_start ||
                offset + 1 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 1;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 1;
            }

            rel = offset - fs->fat_buffer_start;
            entry_bytes = &fs->fat_buffer[rel];

            entry_bytes[0] = value & 0xFF;
            entry_bytes[1] = (value >> 8) & 0xFF;
            return 0;


        case FAT_VERSION_32:
            value &= 0x0FFFFFFF;
            offset = (uint64_t)cluster * 4;

            if (offset < fs->fat_buffer_start ||
                offset + 3 >= fs->fat_buffer_start + FAT_BUFFER_SIZE)
            {
                if (FAT_FlushFATBuffer(fs) != 0) return 1;
                if (FAT_LoadFATBuffer(fs, offset) != 0) return 1;
            }

            rel   = offset - fs->fat_buffer_start;
            entry_bytes = &fs->fat_buffer[rel];

            entry_bytes[0] = value & 0xFF;
            entry_bytes[1] = (value >> 8) & 0xFF;
            entry_bytes[2] = (value >> 16) & 0xFF;
            entry_bytes[3] = (entry_bytes[3] & 0xF0) | ((value >> 24) & 0x0F);

            return 0;

        default:
            break;
    }

    return 1;
}

int FAT_RemoveFATEntries(FAT_File* entry)
{
    if (!entry || entry->read_only || entry->fs->read_only) return 1;

    uint32_t cluster = entry->first_cluster;
    while (FAT_ClusterType(entry->fs, cluster) == FAT_CLUSTER_ALLOCATED) {
        uint32_t next = FAT_ReadFATEntry(entry->fs, cluster);
        if (FAT_WriteFATEntry(entry->fs, cluster, 0) != 0) {
            //TODO
        }
        if (entry->fs->version == FAT_VERSION_32) {
            if (cluster < entry->fs->fs_info.next_free_cluster) entry->fs->fs_info.next_free_cluster = cluster;
            entry->fs->fs_info.free_cluster_count += 1;
        }
        cluster = next;
    }

    return 0;
}


uint32_t FAT_FindNextFreeCluster(FAT_Filesystem* fs, uint32_t start_cluster)
{
    if (!fs || start_cluster > FAT_GetLastCluster(fs)) return 0xFFFFFFFF;
    for (uint32_t c = start_cluster; c <= FAT_GetLastCluster(fs); c++) {
        if (FAT_ReadFATEntry(fs, c) == 0) return c;
    }
    return 0xFFFFFFFF;
}

int FAT_FindFreeClusters(FAT_Filesystem* fs, uint32_t* cluster_array, uint32_t count)
{
    if (!fs || !cluster_array) return 1;

    uint32_t current_count = 0;
    uint32_t last_free_cluster = 0;
    //TODO: if (fs->version == FAT_VERSION_32) last_free_cluster = fs->fs_info.next_free_cluster;
    while (current_count < count) {
        uint32_t next_free_cluster = FAT_FindNextFreeCluster(fs, last_free_cluster);
        if (next_free_cluster > FAT_GetLastCluster(fs)) return 1;

        cluster_array[current_count] = next_free_cluster;
        last_free_cluster = next_free_cluster + 1;
        current_count++;
    }

    return 0;
}

static const int MAX_BOOTSECTOR_ITERATIONS = 25;

int FAT12_FAT16_WriteBootsector(FAT_Filesystem* fs, void* bootsector, int force_bootsector,
                             const char* oem_name, const char* volume_label, uint32_t volume_id,
                             uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                             uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                             uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                             uint8_t media_descriptor, uint32_t hidden_sectors)
{
    if (!fs || fs->read_only) return 1;
    if (fs->version != FAT_VERSION_12 && fs->version != FAT_VERSION_16) return 1;
    if (!force_bootsector && reserved_sectors < 1) return 1; // Bootsector

    if (bootsector) memcpy(&fs->bootsector, bootsector, sizeof(FAT12_FAT16_Bootsector));
    else memset(&fs->bootsector, 0, sizeof(FAT12_FAT16_Bootsector));

    if (!bootsector || !force_bootsector) {
        fs->bootsector.fat12_fat16.signature = 0xAA55;

        memset(fs->bootsector.fat12_fat16.header.oem_name, ' ', 8);
        for (int i = 0; i < 8; i++) {
            if (oem_name[i] == 0) break;
            fs->bootsector.fat12_fat16.header.oem_name[i] = oem_name[i];
        }

        uint64_t total_sectors = total_size / bytes_per_sector;
        uint64_t root_dir_sectors = (max_root_directory_entries * sizeof(FAT_DirectoryEntry) + bytes_per_sector - 1) / bytes_per_sector;

        uint32_t total_clusters = 0;
        uint64_t fat_bytes = 0;
        uint64_t fat_sectors = 1;
        uint64_t data_sectors = total_size / bytes_per_sector - (reserved_sectors + number_of_fats * fat_sectors + root_dir_sectors);

        int iteration = 0;
        while (1) {
            data_sectors = total_sectors - (reserved_sectors + number_of_fats * fat_sectors + root_dir_sectors);
            total_clusters = (uint32_t)(data_sectors / sectors_per_cluster);
            if (total_clusters > FAT_GetMaxClusters(fs)) {
                fprintf(stderr, "Warning: Too many clusters for FAT1%c (%" PRIu32 "). Reducing to max %" PRIu32 ".\n",
                        (fs->version == FAT_VERSION_12 ? '2' : '6'), total_clusters, FAT_GetMaxClusters(fs));
                total_clusters = FAT_GetMaxClusters(fs);
                data_sectors = (uint64_t)total_clusters * sectors_per_cluster;
            }

            switch (fs->version) {
                case FAT_VERSION_12: fat_bytes = (((uint64_t)total_clusters + 2) * 3 + 1) / 2; break;
                case FAT_VERSION_16: fat_bytes = ((uint64_t)total_clusters + 2) * 2; break;
                default: break;
            }
            uint64_t new_fat_sectors = (fat_bytes + bytes_per_sector - 1) / bytes_per_sector;
            if (new_fat_sectors == fat_sectors) break;
            fat_sectors = new_fat_sectors;

            if (iteration++ > MAX_BOOTSECTOR_ITERATIONS) {
                fprintf(stderr, "Max iterations reached for calculating fat_sectors. Using %" PRIu64 "\n", fat_sectors);
                break;
            }
        }

        int use_large_total_sectors = total_sectors > 65535;
        
        fs->bootsector.fat12_fat16.header.bytes_per_sector = (uint16_t)bytes_per_sector;
        fs->bootsector.fat12_fat16.header.sectors_per_cluster = sectors_per_cluster;
        fs->bootsector.fat12_fat16.header.reserved_sectors = reserved_sectors;
        fs->bootsector.fat12_fat16.header.number_of_fats = number_of_fats;
        fs->bootsector.fat12_fat16.header.max_root_directory_entries = max_root_directory_entries;
        fs->bootsector.fat12_fat16.header.total_sectors = (uint16_t)(use_large_total_sectors ? 0 : total_sectors);
        fs->bootsector.fat12_fat16.header.media_descriptor = media_descriptor;
        fs->bootsector.fat12_fat16.header.fat_size = (uint16_t)fat_sectors;
        fs->bootsector.fat12_fat16.header.sectors_per_track = sectors_per_track;
        fs->bootsector.fat12_fat16.header.number_of_heads = number_of_heads;
        fs->bootsector.fat12_fat16.header.hidden_sectors = hidden_sectors;
        fs->bootsector.fat12_fat16.header.large_total_sectors = (use_large_total_sectors ? (uint32_t)total_sectors : 0);
        fs->bootsector.fat12_fat16.header.drive_number = drive_number;
        fs->bootsector.fat12_fat16.header.reserved = 0;
        fs->bootsector.fat12_fat16.header.boot_signature = FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE;

        memset(fs->bootsector.fat12_fat16.header.volume_label, ' ', 11);
        for (int i = 0; i < 11; i++) {
            if (volume_label[i] == 0) break;
            fs->bootsector.fat12_fat16.header.volume_label[i] = volume_label[i];
        }
        fs->bootsector.fat12_fat16.header.filesystem_type[0] = 'F';
        fs->bootsector.fat12_fat16.header.filesystem_type[1] = 'A';
        fs->bootsector.fat12_fat16.header.filesystem_type[2] = 'T';
        fs->bootsector.fat12_fat16.header.filesystem_type[3] = '1';
        switch (fs->version) {
            case FAT_VERSION_12:
                fs->bootsector.fat12_fat16.header.filesystem_type[4] = '2';
                break;
            case FAT_VERSION_16:
                fs->bootsector.fat12_fat16.header.filesystem_type[4] = '6';
                break;
            default:
                break;
        }
        fs->bootsector.fat12_fat16.header.filesystem_type[5] = ' ';
        fs->bootsector.fat12_fat16.header.filesystem_type[6] = ' ';
        fs->bootsector.fat12_fat16.header.filesystem_type[7] = ' ';

        fs->bootsector.fat12_fat16.header.volume_id = volume_id;
    }

    if (!bootsector) {
        fs->bootsector.fat12_fat16.jump[0] = 0xEB;  // jmp short
        fs->bootsector.fat12_fat16.jump[1] = sizeof(FAT12_FAT16_Bootsector_Header) + 1;
        fs->bootsector.fat12_fat16.jump[2] = 0x90;  // nop

        uint8_t code[29] = {0x0E, 0x1F, 0xBE, 0x5B, 0x7C, 0xAC, 0x22, 0xC0, 0x74, 0x0B,
                        0x56, 0xB4, 0x0E, 0xBB, 0x07, 0x00, 0xCD, 0x10, 0x5E, 0xEB,
                        0xF0, 0x32, 0xE4, 0xCD, 0x16, 0xCD, 0x19, 0xEB, 0xFE};

        char data[100] = {
            'T','h','i','s',' ','i','s',' ','n','o','t',' ','a',' ','b','o','o','t','a','b','l','e',' ',
            'd','i','s','k','.',' ',' ','P','l','e','a','s','e',' ','i','n','s','e','r','t',' ','a',' ',
            'b','o','o','t','a','b','l','e',' ','f','l','o','p','p','y',' ','a','n','d','\r','\n',
            'p','r','e','s','s',' ','a','n','y',' ','k','e','y',' ','t','o',' ','t','r','y',' ','a','g',
            'a','i','n',' ','.', '.', '.',' ','\r','\n'
        };

        memset(fs->bootsector.fat12_fat16.bootcode, 0, sizeof(fs->bootsector.fat12_fat16.bootcode));
        memcpy(fs->bootsector.fat12_fat16.bootcode, code, sizeof(code));
        memcpy(fs->bootsector.fat12_fat16.bootcode + sizeof(code), data, sizeof(data));
    }

    //TODO: if force_bootsector check the values and warn

    // Fill area until fat with zeros
    uint64_t current_offset = 0;
    uint64_t to_write = (uint64_t)fs->bootsector.fat12_fat16.header.reserved_sectors * fs->bootsector.fat12_fat16.header.bytes_per_sector;
    uint8_t zero_block[CHUNK_SIZE] = {0};
    while (to_write > 0) {
        uint64_t chunk = to_write < CHUNK_SIZE ? to_write : CHUNK_SIZE;
        if (Partition_Write(fs->partition, (uint8_t*)zero_block, current_offset, chunk) != chunk) {
            // TODO
        }
        current_offset += chunk;
        to_write -= chunk;
    }

    return FAT_WriteBootsector(fs);
}

int FAT32_WriteBootsector(FAT_Filesystem* fs, void* bootsector, int force_bootsector,
                          const char* oem_name, const char* volume_label, uint32_t volume_id,
                          uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                          uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                          uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                          uint8_t media_descriptor, uint32_t hidden_sectors)
{
    if (!fs || fs->read_only) return 1;
    if (fs->version != FAT_VERSION_32) return 1;

    if (!force_bootsector && reserved_sectors < 8) return 1; // Backup boot and fs info

    if (bootsector) memcpy(&fs->bootsector, bootsector, sizeof(FAT32_Bootsector));
    else memset(&fs->bootsector, 0, sizeof(FAT32_Bootsector));

    uint32_t total_clusters = 0;
    if (!bootsector || !force_bootsector) {
        fs->bootsector.fat32.signature = 0xAA55;

        memset(fs->bootsector.fat12_fat16.header.oem_name, ' ', 8);
        for (int i = 0; i < 8; i++) {
            if (oem_name[i] == 0) break;
            fs->bootsector.fat12_fat16.header.oem_name[i] = oem_name[i];
        }

        uint64_t total_sectors = total_size / bytes_per_sector;

        uint64_t fat_bytes = 0;
        uint64_t fat_sectors = 1;
        uint64_t data_sectors = total_size / bytes_per_sector - (reserved_sectors + number_of_fats * fat_sectors);

        int iteration = 0;
        while (1) {
            data_sectors = total_size / bytes_per_sector - (reserved_sectors + number_of_fats * fat_sectors);
            total_clusters = (uint32_t)(data_sectors / sectors_per_cluster);
            if (total_clusters > FAT_GetMaxClusters(fs)) {
                fprintf(stderr, "Warning: Too many clusters for FAT32 (%" PRIu32 "). Reducing to max %" PRIu32 ".\n",
                        total_clusters, FAT_GetMaxClusters(fs));
                total_clusters = FAT_GetMaxClusters(fs);
                data_sectors = (uint64_t)total_clusters * sectors_per_cluster;
            }

            fat_bytes = ((uint64_t)total_clusters + 2) * 4;
            uint64_t new_fat_sectors = (fat_bytes + bytes_per_sector - 1) / bytes_per_sector;
            if (new_fat_sectors == fat_sectors) break;
            fat_sectors = new_fat_sectors;

            if (iteration++ > MAX_BOOTSECTOR_ITERATIONS) {
                fprintf(stderr, "Max iterations reached for calculating fat_sectors. Using %" PRIu64 "\n", fat_sectors);
                break;
            }
        }

        int use_large_total_sectors = 1; // TODO

        fs->bootsector.fat32.header.bytes_per_sector = (uint16_t)bytes_per_sector;
        fs->bootsector.fat32.header.sectors_per_cluster = sectors_per_cluster;
        fs->bootsector.fat32.header.reserved_sectors = reserved_sectors;
        fs->bootsector.fat32.header.number_of_fats = number_of_fats;
        fs->bootsector.fat32.header.max_root_directory_entries = max_root_directory_entries;
        fs->bootsector.fat32.header.total_sectors_small = (uint16_t)(use_large_total_sectors ? 0 : total_sectors);
        fs->bootsector.fat32.header.media_descriptor = media_descriptor;
        fs->bootsector.fat32.header.fat_size_small = 0; //FAT32
        fs->bootsector.fat32.header.sectors_per_track = sectors_per_track;
        fs->bootsector.fat32.header.number_of_heads = number_of_heads;
        fs->bootsector.fat32.header.hidden_sectors = hidden_sectors;
        fs->bootsector.fat32.header.total_sectors_large = (uint32_t)(use_large_total_sectors ? total_sectors : 0);
        fs->bootsector.fat32.header.drive_number = drive_number;
        fs->bootsector.fat32.header.reserved1 = 0;
        fs->bootsector.fat32.header.boot_signature = FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE;

        fs->bootsector.fat32.header.fat_size_32 = (uint32_t)fat_sectors;
        fs->bootsector.fat32.header.ext_flags = 0; //TODO
        fs->bootsector.fat32.header.fs_version = 0; //TODO

        memset(fs->bootsector.fat32.header.volume_label, ' ', 11);
        for (int i = 0; i < 11; i++) {
            if (volume_label[i] == 0) break;
            fs->bootsector.fat32.header.volume_label[i] = volume_label[i];
        }
        fs->bootsector.fat32.header.filesystem_type[0] = 'F';
        fs->bootsector.fat32.header.filesystem_type[1] = 'A';
        fs->bootsector.fat32.header.filesystem_type[2] = 'T';
        fs->bootsector.fat32.header.filesystem_type[3] = '3';
        fs->bootsector.fat32.header.filesystem_type[4] = '2';
        fs->bootsector.fat32.header.filesystem_type[5] = ' ';
        fs->bootsector.fat32.header.filesystem_type[6] = ' ';
        fs->bootsector.fat32.header.filesystem_type[7] = ' ';

        fs->bootsector.fat32.header.volume_id = volume_id;

        fs->bootsector.fat32.header.root_cluster = 0;
        memset(fs->bootsector.fat32.header.reserved, 0, sizeof(fs->bootsector.fat32.header.reserved));

        fs->bootsector.fat32.header.fs_info_sector = 1;
        fs->bootsector.fat32.header.backup_boot_sector = 6;
    } else {
        uint64_t total_sectors = (fs->bootsector.fat32.header.total_sectors_large != 0) ? fs->bootsector.fat32.header.total_sectors_large : fs->bootsector.fat32.header.total_sectors_small;
        uint64_t fat_size = (fs->bootsector.fat32.header.fat_size_32 != 0) ? fs->bootsector.fat32.header.fat_size_32 : fs->bootsector.fat32.header.fat_size_small;
        uint64_t data_sectors = total_sectors - (fs->bootsector.fat32.header.reserved_sectors + (fat_size * fs->bootsector.fat32.header.number_of_fats));
        total_clusters = (uint32_t)(data_sectors / fs->bootsector.fat32.header.sectors_per_cluster);
    }

    if (!bootsector) {
        fs->bootsector.fat32.jump[0] = 0xEB;  // jmp short
        fs->bootsector.fat32.jump[1] = sizeof(FAT32_Bootsector_Header) + 1;
        fs->bootsector.fat32.jump[2] = 0x90;  // nop

        uint8_t code[29] = {0x0E, 0x1F, 0xBE, 0x77, 0x7C, 0xAC, 0x22, 0xC0, 0x74, 0x0B,
                        0x56, 0xB4, 0x0E, 0xBB, 0x07, 0x00, 0xCD, 0x10, 0x5E, 0xEB,
                        0xF0, 0x32, 0xE4, 0xCD, 0x16, 0xCD, 0x19, 0xEB, 0xFE};

        char data[100] = {
            'T','h','i','s',' ','i','s',' ','n','o','t',' ','a',' ','b','o','o','t','a','b','l','e',' ',
            'd','i','s','k','.',' ',' ','P','l','e','a','s','e',' ','i','n','s','e','r','t',' ','a',' ',
            'b','o','o','t','a','b','l','e',' ','f','l','o','p','p','y',' ','a','n','d','\r','\n',
            'p','r','e','s','s',' ','a','n','y',' ','k','e','y',' ','t','o',' ','t','r','y',' ','a','g',
            'a','i','n',' ','.', '.', '.',' ','\r','\n'
        };

        memset(fs->bootsector.fat32.bootcode, 0, sizeof(fs->bootsector.fat32.bootcode));
        memcpy(fs->bootsector.fat32.bootcode, code, sizeof(code));
        memcpy(fs->bootsector.fat32.bootcode + sizeof(code), data, sizeof(data));
    }

    //TODO: if force_bootsector check the values and warn

    // Fill area until fat with zeros
    uint64_t current_offset = 0;
    uint64_t to_write = (uint64_t)fs->bootsector.fat32.header.reserved_sectors * fs->bootsector.fat32.header.bytes_per_sector;
    uint8_t zero_block[CHUNK_SIZE] = {0};
    while (to_write > 0) {
        uint64_t chunk = to_write < CHUNK_SIZE ? to_write : CHUNK_SIZE;
        if (Partition_Write(fs->partition, (uint8_t*)zero_block, current_offset, chunk) != chunk) {
            // TODO
        }
        current_offset += chunk;
        to_write -= chunk;
    }

    if (FAT_WriteBootsector(fs) != 0) return 1;

    memset(&fs->fs_info, 0, sizeof(FAT32_FS_Info));

    fs->fs_info.lead_signature = 0x41615252;
    fs->fs_info.struct_signature = 0x61417272;
    fs->fs_info.trail_signature = 0xAA550000;

    fs->fs_info.free_cluster_count = total_clusters;
    fs->fs_info.next_free_cluster = 1;

    return FAT32_WriteFSInfo(fs);
}

int FAT_WriteEmptyFAT(FAT_Filesystem* fs)
{
    if (!fs || fs->read_only) return 1;
    if (fs->version != FAT_VERSION_12 && fs->version != FAT_VERSION_16 && fs->version != FAT_VERSION_32) return 1;

    uint64_t offset = fs->fat_offset;

    uint64_t fat_size = fs->fat_size;
    uint64_t written = 0;

    uint8_t media_descriptor = fs->version == FAT_VERSION_32 ? fs->bootsector.fat32.header.media_descriptor : fs->bootsector.fat12_fat16.header.media_descriptor;
    uint8_t header[8] = {media_descriptor, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    switch (fs->version) {
        case FAT_VERSION_12:
            if (Partition_Write(fs->partition, header, offset, 3) != 3) return 1;
            offset += 3;
            written += 3;
            break;
        case FAT_VERSION_16:
            if (Partition_Write(fs->partition, header, offset, 4) != 4) return 1;
            offset += 4;
            written += 4;
            break;
        case FAT_VERSION_32:
            header[3] = 0x0F;
            header[7] = 0x0F;
            if (Partition_Write(fs->partition, header, offset, 8) != 8) return 1;
            offset += 8;
            written += 8;
            break;
        default:
            break;
    }

    uint8_t zero_block[CHUNK_SIZE] = {0};
    while (written < fat_size) {
        uint64_t chunk = (fat_size - written) < CHUNK_SIZE ? (fat_size - written) : CHUNK_SIZE;
        if (Partition_Write(fs->partition, (uint8_t*)zero_block, offset, chunk) != chunk) return 1;
        offset += chunk;
        written += chunk;
    }

    return 0;
}

int FAT_CopyFAT(FAT_Filesystem* fs, uint8_t dst, uint8_t src)
{
    if (!fs || fs->read_only) return 1;

    uint64_t offset_dst = fs->fat_offset + fs->fat_size * dst;
    uint64_t offset_src = fs->fat_offset + fs->fat_size * src;

    uint8_t buf[CHUNK_SIZE];
    uint64_t remaining = fs->fat_size;

    while (remaining > 0) {
        uint64_t to_copy = ((remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE);

        if (Partition_Read(fs->partition, (uint8_t*)buf, offset_src, to_copy) != to_copy) return 1;
        if (Partition_Write(fs->partition, (uint8_t*)buf, offset_dst, to_copy) != to_copy) return 1;

        offset_src += to_copy;
        offset_dst += to_copy;
        remaining -= to_copy;
    }

    return 0;
}

int FAT_WriteEmptyRootDir(FAT_Filesystem* fs)
{
    if (!fs || fs->read_only) return 1;

    uint64_t offset = fs->root_offset;
    uint8_t zero_block[CHUNK_SIZE] = {0};
    uint64_t written = 0;

    while (written < fs->root_size) {
        uint64_t chunk = (fs->root_size - written) < CHUNK_SIZE ? (fs->root_size - written) : CHUNK_SIZE;
        if (Partition_Write(fs->partition, (uint8_t*)zero_block, offset, chunk) != chunk) return 1;
        offset += chunk;
        written += chunk;
    }

    return 0;
}

int FAT_WriteBootsector(FAT_Filesystem* fs)
{
    if (!fs || fs->read_only) return 1;

    uint64_t written = Partition_Write(fs->partition, (uint8_t*)&fs->bootsector, 0, sizeof(FAT_Bootsector));
    if (written != sizeof(FAT_Bootsector)) {
        return 1;
    }

    if (fs->version == FAT_VERSION_32) {
        uint64_t offset = (uint64_t)fs->bootsector.fat32.header.backup_boot_sector * fs->bootsector.fat32.header.bytes_per_sector;
        written = Partition_Write(fs->partition, (uint8_t*)&fs->bootsector, offset, sizeof(FAT_Bootsector));
        if (written != sizeof(FAT_Bootsector)) {
            return 1;
        }
    }

    return 0;
}

int FAT32_WriteFSInfo(FAT_Filesystem* fs)
{
    if (!fs || fs->read_only || fs->version != FAT_VERSION_32) return 1;

    uint64_t offset = (uint64_t)fs->bootsector.fat32.header.fs_info_sector * fs->bootsector.fat32.header.bytes_per_sector;

    uint64_t written = Partition_Write(fs->partition, (uint8_t*)&fs->fs_info, offset, sizeof(FAT32_FS_Info));
    if (written != sizeof(FAT32_FS_Info)) {
        return 1;
    }

    // Backup fs_info too
    offset += (uint64_t)fs->bootsector.fat32.header.backup_boot_sector * fs->bootsector.fat32.header.bytes_per_sector;

    written = Partition_Write(fs->partition, (uint8_t*)&fs->fs_info, offset, sizeof(FAT32_FS_Info));
    if (written != sizeof(FAT32_FS_Info)) {
        return 1;
    }

    return 0;
}
