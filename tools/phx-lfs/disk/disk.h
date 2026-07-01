#pragma once

#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
    #define FileSeek _fseeki64
    #define FileTell _ftelli64
    typedef int64_t file_offset_t;
#else
    #define FileSeek fseeko
    #define FileTell ftello
    #include <sys/types.h>
    typedef off_t file_offset_t;
#endif

typedef unsigned char DiskFormat;
#define DISKFORMAT_RAW ((DiskFormat)0)

typedef struct Disk {
    FILE* f;
    
    uint64_t current_position;
    uint64_t size;

    uint64_t sectorSize;

    DiskFormat format;
} Disk;

uint64_t Disk_Read(Disk* disk, void* buffer, uint64_t offset, uint64_t size);
uint64_t Disk_Write(Disk* disk, void* buffer, uint64_t offset, uint64_t size);
Disk* Disk_CreateFromFile(FILE* raw, uint64_t max_size, DiskFormat format);
Disk* Disk_OpenFromFile(FILE* raw, uint64_t max_size, DiskFormat format);
void Disk_Close(Disk* disk);

uint64_t Path_GetSize(const char* path);
