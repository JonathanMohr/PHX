#pragma once

#include <stdint.h>
#include "../fat/fat.h"

typedef unsigned char Filesystem_Type;
#define FILESYSTEM_FAT12    ((Filesystem_Type)1)
#define FILESYSTEM_FAT16    ((Filesystem_Type)2)
#define FILESYSTEM_FAT32    ((Filesystem_Type)3)

char** SeparatePaths(const char* path, uint32_t* out_count);

typedef struct Filesystem Filesystem;

typedef struct Filesystem_File {
    Filesystem* fs;
    FAT_File* fat_f;
} Filesystem_File;

struct Filesystem {
    FAT_Filesystem* fat_fs;
    int fat_use_lfn;

    Filesystem_File* root;
    Filesystem_File static_root;
};

Filesystem* Filesystem_CreateFromFAT(FAT_Filesystem* fat_fs, int use_lfn);
void Filesystem_Close(Filesystem* fs);

Filesystem_File* Filesystem_CreateEntry(Filesystem_File* parent, const char* name, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access);
Filesystem_File* Filesystem_FindEntry(Filesystem_File* parent, const char* name);

Filesystem_File* Filesystem_OpenEntry(Filesystem_File* parent, const char* name, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access);

int Filesystem_DeleteEntry(Filesystem_File* parent, const char* name);

Filesystem_File* Filesystem_OpenPath(Filesystem_File* current_path, const char* path, int create_file, int create_parents, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access);
int Filesystem_DeletePath(Filesystem_File* current_path, const char* path, int save);
void Filesystem_CloseEntry(Filesystem_File* file);

uint64_t Filesystem_ReadFromFile(Filesystem_File* file, uint64_t offset, uint8_t* buffer, uint64_t size);
uint64_t Filesystem_WriteToFile(Filesystem_File* file, uint64_t offset, uint8_t* buffer, uint64_t size);

char** Filesystem_ListDir(Filesystem_File* file, uint64_t* out_count);

int Filesystem_DirectoryReserve(Filesystem_File* dir, char** file_names, uint64_t file_count);

int Filesystem_SyncPathsToFS(Filesystem_File* dir, const char* path, const char* o_path);
int Filesystem_SyncPathsFromFS(Filesystem_File* dir, const char* path, const char* o_path);

int Filesystem_PrintAll(Filesystem_File* start_dir, const char* name, int indent);
