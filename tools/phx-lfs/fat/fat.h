#pragma once

#include <stdint.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>
#include "../partition/partition.h"

#include <toolchain.h>

#define CHUNK_SIZE 16384 /*512*/

#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY144     ((uint8_t)0xF0) // 1.44 MB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY120     ((uint8_t)0xF4) // 1.2 MB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY720     ((uint8_t)0xF9) // 720 KB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY400     ((uint8_t)0xFD) // 400 KB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY360_OLD ((uint8_t)0xFF) // 360 KB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY360     ((uint8_t)0xF6) // 360 KB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY320     ((uint8_t)0xF7) // 320 KB
#define FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_DISK          ((uint8_t)0xF8)

#define FAT_BOOTSECTOR_NO_EXTENDED_BOOT_SIGNATURE     ((uint8_t)0x00)
#define FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE_OLD    ((uint8_t)0x28)
#define FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE        ((uint8_t)0x29)

#define FAT_BUFFER_SIZE 1024

#define FAT_ENTRY_DELETED ((char)0xE5)

#define FAT_ENTRY_READ_ONLY     ((uint8_t)0x01)
#define FAT_ENTRY_HIDDEN        ((uint8_t)0x02)
#define FAT_ENTRY_SYSTEM        ((uint8_t)0x04)
#define FAT_ENTRY_VOLUME_LABEL  ((uint8_t)0x08)
#define FAT_ENTRY_DIRECTORY     ((uint8_t)0x10)
#define FAT_ENTRY_ARCHIVE       ((uint8_t)0x20)

PACKED_BEGIN

typedef struct FAT12_FAT16_Bootsector_Header {
    char oem_name[8];
    
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;

    uint8_t number_of_fats;
    uint16_t max_root_directory_entries;
    uint16_t total_sectors;

    uint8_t media_descriptor;

    uint16_t fat_size; // in sectors

    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    
    uint32_t hidden_sectors;
    uint32_t large_total_sectors; // if total_sectors > 65535, else 0

    uint8_t drive_number;

    uint8_t reserved;
    uint8_t boot_signature;

    // Only when FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE
    uint32_t volume_id;

    // Only when FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE or FAT_BOOTSECTOR_EXTENDED_BOOT_SIGNATURE_OLD
    char volume_label[11];
    char filesystem_type[8];

} __attribute__((packed)) FAT12_FAT16_Bootsector_Header;

typedef struct FAT32_Bootsector_Header {
    char     oem_name[8];

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;

    uint8_t  number_of_fats;
    uint16_t max_root_directory_entries;
    uint16_t total_sectors_small;

    uint8_t  media_descriptor;

    uint16_t fat_size_small;

    uint16_t sectors_per_track;
    uint16_t number_of_heads;

    uint32_t hidden_sectors;
    uint32_t total_sectors_large;

    // FAT32 Extended BPB
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];

    uint8_t  drive_number;

    uint8_t  reserved1;
    uint8_t  boot_signature;

    uint32_t volume_id;

    char     volume_label[11];
    char     filesystem_type[8];
} __attribute__((packed)) FAT32_Bootsector_Header;

#define FAT_BOOTSECTOR_SIGNATURE 0x55AA

typedef struct FAT12_FAT16_Bootsector {
    uint8_t jump[3];
    FAT12_FAT16_Bootsector_Header header;
    uint8_t bootcode[448];
    uint16_t signature;
} __attribute__((packed)) FAT12_FAT16_Bootsector;

typedef struct FAT32_Bootsector {
    uint8_t jump[3];
    FAT32_Bootsector_Header header;
    uint8_t bootcode[420];
    uint16_t signature;
} __attribute__((packed)) FAT32_Bootsector;

PACKED_END

typedef struct FAT32_FS_Info {
    uint32_t lead_signature;
    uint8_t reserved[480];
    uint32_t struct_signature;
    uint32_t free_cluster_count;
    uint32_t next_free_cluster;
    uint8_t reserved2[12];
    uint32_t trail_signature;
} FAT32_FS_Info;

PACKED_BEGIN

typedef struct FAT_DirectoryEntry {
    char name[8];
    char ext[3];

    uint8_t attribute;

    uint8_t reserved;

    uint8_t creation_time_tenths; // tenths of second
    uint16_t creation_time; // HHMMSS (Hour-Minute-Second)
    uint16_t creation_date;

    uint16_t last_access_date;
    
    uint16_t first_cluster_high; // Only FAT32

    uint16_t last_modification_time;
    uint16_t last_modification_date;

    uint16_t first_cluster; // low word

    uint32_t file_size; // bytes

} __attribute__((packed)) FAT_DirectoryEntry;

typedef struct FAT_LFNEntry {
    uint8_t order;

    uint16_t name1[5];

    uint8_t attr;
    uint8_t reserved;
    uint8_t checksum;

    uint16_t name2[6];

    uint16_t reserved2;

    uint16_t name3[2];

} __attribute__((packed)) FAT_LFNEntry;

PACKED_END

typedef struct FAT_Filesystem FAT_Filesystem;

typedef struct FAT_File {
    FAT_Filesystem* fs;

    uint64_t size;

    uint32_t first_cluster;

    uint64_t lfn_offset;
    uint64_t directory_entry_offset; // absolute in bytes from file start

    int is_root_directory;
    int is_directory;
    int is_root_directory_fat32;
    int read_only;
    int changed;

    int is_hidden;
    int is_system;

} FAT_File;

typedef union FAT_Bootsector {
    uint8_t buffer[512];
    FAT12_FAT16_Bootsector fat12_fat16;
    FAT32_Bootsector fat32;
} FAT_Bootsector;

typedef uint8_t Fat_Version;
#define FAT_VERSION_12 ((Fat_Version)1)
#define FAT_VERSION_16 ((Fat_Version)2)
#define FAT_VERSION_32 ((Fat_Version)3)

struct FAT_Filesystem {
    Fat_Version version;

    Partition* partition;
    
    FAT_Bootsector bootsector;

    FAT_File* root;
    FAT_File static_root;

    uint8_t fat_buffer[FAT_BUFFER_SIZE];
    uint64_t fat_buffer_start;

    uint64_t size; // in bytes

    uint64_t fat_offset;    // in bytes
    uint64_t fat_size;      // in bytes

    uint64_t root_offset;   // in bytes
    uint64_t root_size;     // in bytes

    uint64_t data_offset;   // in bytes
    uint64_t data_size;     // in bytes

    uint64_t cluster_size;  // in bytes

    // FAT32 only
    FAT32_FS_Info fs_info;

    int read_only;

};

static inline uint32_t FAT_GetMaxClusters(FAT_Filesystem* fs)
{
    switch (fs->version) {
        case FAT_VERSION_12: return 4084;
        case FAT_VERSION_16: return 65524;
        case FAT_VERSION_32: return 268435444;
        default: return 0;
    }
}

static inline uint32_t FAT_GetLastCluster(FAT_Filesystem* fs)
{
    switch (fs->version) {
        case FAT_VERSION_12: return 0xFEF;
        case FAT_VERSION_16: return 0xFFEF;
        case FAT_VERSION_32: return 0x0FFFFFEF;
        default: return 0;
    }
}

static inline uint32_t FAT_GetEOF(FAT_Filesystem* fs)
{
    switch (fs->version) {
        case FAT_VERSION_12: return 0xFF8;
        case FAT_VERSION_16: return 0xFFF8;
        case FAT_VERSION_32: return 0x0FFFFFF8;
        default: return 0;
    }
}

typedef uint8_t FAT_ClusterState;
#define FAT_CLUSTER_UNKNOWN     ((FAT_ClusterState)0)
#define FAT_CLUSTER_FREE        ((FAT_ClusterState)1)
#define FAT_CLUSTER_ALLOCATED   ((FAT_ClusterState)2)
#define FAT_CLUSTER_RESERVED    ((FAT_ClusterState)3)
#define FAT_CLUSTER_BAD_CLUSTER ((FAT_ClusterState)4)
#define FAT_CLUSTER_EOC         ((FAT_ClusterState)5)

static inline FAT_ClusterState FAT_ClusterType(FAT_Filesystem* fs, uint32_t value)
{
    switch (fs->version) {

        case FAT_VERSION_12:
            if (value == 0x000) return FAT_CLUSTER_FREE;
            if (value == 0x001) return FAT_CLUSTER_RESERVED;
            if (value >= 0xFF0 && value <= 0xFF6) return FAT_CLUSTER_RESERVED;
            if (value == 0xFF7) return FAT_CLUSTER_BAD_CLUSTER;
            if (value >= 0xFF8 && value <= 0xFFF) return FAT_CLUSTER_EOC;
            return FAT_CLUSTER_ALLOCATED;

        case FAT_VERSION_16:
            if (value == 0x0000) return FAT_CLUSTER_FREE;
            if (value == 0x0001) return FAT_CLUSTER_RESERVED;
            if (value >= 0xFFF0 && value <= 0xFFF6) return FAT_CLUSTER_RESERVED;
            if (value == 0xFFF7) return FAT_CLUSTER_BAD_CLUSTER;
            if (value >= 0xFFF8 && value <= 0xFFFF) return FAT_CLUSTER_EOC;
            return FAT_CLUSTER_ALLOCATED;

        case FAT_VERSION_32:
            value &= 0x0FFFFFFF;
            if (value == 0x0000000) return FAT_CLUSTER_FREE;
            if (value == 0x0000001) return FAT_CLUSTER_RESERVED;
            if (value >= 0x0FFFFFF0 && value <= 0x0FFFFFF6) return FAT_CLUSTER_RESERVED;
            if (value == 0x0FFFFFF7) return FAT_CLUSTER_BAD_CLUSTER;
            if (value >= 0x0FFFFFF8 && value <= 0x0FFFFFFF) return FAT_CLUSTER_EOC;
            return FAT_CLUSTER_ALLOCATED;

        default: break;
    }
    return FAT_CLUSTER_UNKNOWN;
}

// Functions:

int FAT_ParseName(const char* name, char fat_name[8], char fat_ext[3]);
FAT_LFNEntry* FAT_CreateLFNEntries(const char* name, uint32_t* out_count, uint8_t checksum);
uint16_t* FAT_CombineLFN(FAT_LFNEntry* entries, uint32_t entry_count, uint32_t* len);
uint8_t FAT_GetChecksum(const char shortname[11]);
static inline uint8_t FAT_CreateChecksum(FAT_DirectoryEntry* entry)
{
    char full[11];
    for (int i = 0; i < 8; i++) full[i] = entry->name[i];
    for (int i = 0; i < 3; i++) full[i+8] = entry->ext[i];
    return FAT_GetChecksum(full);
}

void FAT_EncodeTime(int64_t epoch, uint16_t* fat_date, uint16_t* fat_time, uint8_t* tenths);

int FAT_FlushFATBuffer(FAT_Filesystem* fs);
int FAT_LoadFATBuffer(FAT_Filesystem* fs, uint64_t offset);

uint64_t FAT_ReadFromFileRaw(FAT_File* f, uint64_t offset, void* buffer, uint64_t size);
uint64_t FAT_WriteToFileRaw(FAT_File* f, uint64_t offset, void* buffer, uint64_t size);
int FAT_ReserveSpace(FAT_File* f, uint64_t extra, int update_entry_size);
int FAT_ReserveDirectorySpace(FAT_File* dir, uint64_t entry_count);
uint64_t FAT_GetAbsoluteOffset(FAT_File* f, uint64_t relative_offset);

uint64_t FAT_AddDirectoryEntry(FAT_File* directory, FAT_DirectoryEntry* entry, FAT_LFNEntry* lfn_entries, uint64_t lfn_count);
int FAT_AddDotsToDirectory(FAT_File* directory, FAT_File* parent);

int FAT_GetDirectoryEntry(FAT_File* f, FAT_DirectoryEntry* entry);
int FAT_SetDirectoryEntry(FAT_File* f, FAT_DirectoryEntry* entry);
int FAT_RemoveDirectoryEntry(FAT_File* f);

FAT_File* FAT_CreateEntryRaw(FAT_File* dir, FAT_DirectoryEntry* entry, int is_directory, FAT_LFNEntry* lfn_entries, uint32_t lfn_count);
FAT_File* FAT_CreateEntry(FAT_File* parent, const char* name, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access, int use_lfn);
int FAT_DeleteEntry(FAT_File* f);
void FAT_CloseEntry(FAT_File* entry);

FAT_File* FAT_FindEntry(FAT_File* parent, const char* name);

static inline uint64_t FAT_ReadFromFile(FAT_File* f, uint64_t offset, void* buffer, uint64_t size)
{
    if (!f || f->is_directory) return 0;
    return FAT_ReadFromFileRaw(f, offset, buffer, size);
}

static inline uint64_t FAT_WriteToFile(FAT_File* f, uint64_t offset, void* buffer, uint64_t size)
{
    if (!f || f->is_directory) return 0;
    return FAT_WriteToFileRaw(f, offset, buffer, size);
}

void FAT_DumpInfo(FAT_Filesystem* fs, FILE* s, int count_fat);

// Initializes an empty FAT Filesystem
FAT_Filesystem* FAT_CreateEmptyFilesystem(Partition* partition, Fat_Version version, int fast, void* bootsector, int force_bootsector,
                                          const char* oem_name, const char* volume_label, uint32_t volume_id,
                                          uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                                          uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                                          uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                                          uint8_t media_descriptor, uint32_t hidden_sectors);

// Reads an existing FAT Filesystem
FAT_Filesystem* FAT_OpenFilesystem(Partition* partition, Fat_Version version, int read_only);

void FAT_CloseFilesystem(FAT_Filesystem* fs);


uint32_t FAT_ReadFATEntry(FAT_Filesystem* fs, uint32_t cluster);
int FAT_WriteFATEntry(FAT_Filesystem* fs, uint32_t cluster, uint32_t value);
int FAT_RemoveFATEntries(FAT_File* entry);

uint32_t FAT_FindNextFreeCluster(FAT_Filesystem* fs, uint32_t start_cluster);
int FAT_FindFreeClusters(FAT_Filesystem* fs, uint32_t* cluster_array, uint32_t count);

int FAT_WriteBootsector(FAT_Filesystem* fs);
int FAT32_WriteFSInfo(FAT_Filesystem* fs);
int FAT_CopyFAT(FAT_Filesystem* fs, uint8_t dst, uint8_t src);

char** FAT_ListDir(FAT_File* dir, uint64_t* out_count);

// EmptyFS:

int FAT12_FAT16_WriteBootsector(FAT_Filesystem* fs, void* bootsector, int force_bootsector,
                                const char* oem_name, const char* volume_label, uint32_t volume_id,
                                uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                                uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                                uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                                uint8_t media_descriptor, uint32_t hidden_sectors);

int FAT32_WriteBootsector(FAT_Filesystem* fs, void* bootsector, int force_bootsector,
                          const char* oem_name, const char* volume_label, uint32_t volume_id,
                          uint64_t total_size, uint32_t bytes_per_sector, uint8_t sectors_per_cluster,
                          uint16_t reserved_sectors, uint8_t number_of_fats, uint16_t max_root_directory_entries,
                          uint16_t sectors_per_track, uint16_t number_of_heads, uint8_t drive_number,
                          uint8_t media_descriptor, uint32_t hidden_sectors);

// Seeks to fs->fat_offset and writes FAT
// Parameters:
//   fs     - FAT_Filesystem struct
int FAT_WriteEmptyFAT(FAT_Filesystem* fs);

// Creates an empty root directory
// Parameters:
//   fs     - FAT_Filesystem struct
int FAT_WriteEmptyRootDir(FAT_Filesystem* fs);

static inline uint32_t utf8_to_utf16(const char* input, uint16_t** out)
{
    uint32_t len = (uint32_t)strlen(input);
    uint16_t* buf = malloc(sizeof(uint16_t) * (len + 1) * 2);
    if (!buf) return 0;
    uint32_t outlen = 0;

    const uint8_t* p = (const uint8_t*)input;

    while (*p)
    {
        uint32_t code = 0;

        if ((*p & 0x80) == 0x00) {
            code = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            code = (uint32_t)(((*p & 0x1F) << 6) | (p[1] & 0x3F));
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            code = (uint32_t)(((*p & 0x0F) << 12) |
                             ((p[1] & 0x3F) << 6) |
                             (p[2] & 0x3F));
            p += 3;
        } else {
            // unsupported → replace
            code = '?';
            p++;
        }

        buf[outlen++] = (uint16_t)code;
    }

    *out = buf;
    return outlen;
}

static inline uint32_t utf16_to_utf8(const uint16_t* input, uint32_t inlen, char** out)
{
    if (!input) return 0;

    char* buf = malloc((inlen * 3) + 1);
    if (!buf) return 0;

    uint32_t outlen = 0;

    for (uint32_t i = 0; i < inlen; i++) {
        uint32_t code = input[i];

        if (code == 0x0000) break;

        if (code <= 0x7F) {
            buf[outlen++] = (char)code;
        } else if (code <= 0x7FF) {
            buf[outlen++] = (char)(0xC0 | ((code >> 6) & 0x1F));
            buf[outlen++] = (char)(0x80 | (code & 0x3F));
        } else {
            buf[outlen++] = (char)(0xE0 | ((code >> 12) & 0x0F));
            buf[outlen++] = (char)(0x80 | ((code >> 6) & 0x3F));
            buf[outlen++] = (char)(0x80 | (code & 0x3F));
        }
    }

    buf[outlen] = '\0';
    *out = buf;
    return outlen;
}

static inline int utf16_case_insensitive_equal(const uint16_t* name1, const uint16_t* name2, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (towupper(name1[i]) != towupper(name2[i])) {
            return 0;
        }
    }
    return 1;
}
