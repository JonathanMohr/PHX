#include "disk.h"

#include <stdlib.h>

uint64_t Disk_Read(Disk* disk, void* buffer, uint64_t offset, uint64_t size)
{
    if (!disk || !buffer || offset >= disk->size || size == 0) return 0;

    if (offset != disk->current_position) {
        if (FileSeek(disk->f, (file_offset_t)offset, SEEK_SET) != 0) return 0;
        disk->current_position = offset;
    }

    if (offset + size > disk->size) size = disk->size - offset;

    uint64_t read = fread(buffer, 1, size, disk->f);
    disk->current_position += read;
    return read;
}

uint64_t Disk_Write(Disk* disk, void* buffer, uint64_t offset, uint64_t size)
{
    if (!disk || !buffer /*|| offset >= disk->size*/ || size == 0) return 0;

    if (offset != disk->current_position) {
        if (FileSeek(disk->f, (file_offset_t)offset, SEEK_SET) != 0) return 0;
        disk->current_position = offset;
    }

    // if (offset + size > disk->size) size = disk->size - offset;

    uint64_t written = fwrite(buffer, 1, size, disk->f);

    if (fflush(disk->f) != 0) {
        perror("fflush failed");
    }

    if (offset + written > disk->size) disk->size = offset + written;
    disk->current_position += written;

    return written;
}

Disk* Disk_CreateFromFile(FILE* raw, uint64_t max_size, DiskFormat format)
{
    if (FileSeek(raw, 0, SEEK_SET) != 0) return NULL;

    Disk* disk = (Disk*)malloc(sizeof(Disk));
    if (!disk) return NULL;
    disk->f = raw;

    disk->size = max_size;

    disk->current_position = 0;

    disk->sectorSize = 512; // TODO

    disk->format = format;

    return disk;
}

Disk* Disk_OpenFromFile(FILE* raw, uint64_t max_size, DiskFormat format)
{
    if (FileSeek(raw, 0, SEEK_SET) != 0) return NULL;

    Disk* disk = (Disk*)malloc(sizeof(Disk));
    if (!disk) return NULL;
    disk->f = raw;

    disk->size = max_size;

    disk->current_position = 0;

    disk->sectorSize = 512; // TODO

    disk->format = format;

    return disk;
}

void Disk_Close(Disk* disk)
{
    fclose(disk->f);
    free(disk);
}
