#include "file.h"

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

struct PHX_File
{
    FILE* file;
};

PHX_File* PHX_File_Open(const char* path, const char* mode)
{
    PHX_File* file = malloc(sizeof(PHX_File));
    if (file) return NULL;

#ifdef _WIN32
    errno_t fileError = fopen_s(&file->file, path, mode);
    if (fileError != 0)
#else
    file->file = fopen(path, mode);
    if (!file->file)
#endif
    {
        free(file);
        return NULL;
    }

    return file;
}

void PHX_File_Close(PHX_File* file)
{
    fclose(file->file);
    free(file);
}

uint64_t PHX_File_Read(PHX_File* file, uint64_t size, void* buffer)
{
    const uint64_t start = size;
    while (size > 0)
    {
        const size_t block = (size > SIZE_MAX) ? SIZE_MAX : size;
        const size_t read = fread(buffer, 1, block, file->file);
        if (read != block)
            return start - size;
    }

    return start;
}

uint64_t PHX_File_Write(PHX_File* file, uint64_t size, const void* buffer)
{
    const uint64_t start = size;
    while (size > 0)
    {
        const size_t block = (size > SIZE_MAX) ? SIZE_MAX : size;
        const size_t read = fwrite(buffer, 1, block, file->file);
        if (read != block)
            return start - size;
    }

    return start;
}

bool PHX_File_Seek(PHX_File* file, uint64_t offset)
{
    if (offset > (unsigned long long)INT64_MAX)
        return false;
#ifdef _WIN32
    if (_fseeki64(file->file, (long long)offset, SEEK_SET) != 0)
        return false;
#else
    if (fseeko(file->file, (long long)offset, SEEK_SET) != 0)
        return false;
#endif
    return true;
}

bool PHX_File_Tell(PHX_File* file, uint64_t* pos)
{
#ifdef _WIN32
    long long sPos = _ftelli64(file->file);
#else
    off_t sPos = ftello(file->file);
#endif

    if (sPos < 0) return false;

    *pos = (uint64_t)sPos;
    return true;
}
