#include "file.h"

#include <stdio.h>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/stat.h>
#endif

struct PHX_File
{
    FILE* file;
};

static PHX_Bool get_file_size(const char* path, PHX_BlockSize* out)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
        return PHX_FALSE;
    const uint64_t size = (uint64_t)fad.nFileSizeHigh << 32 | (uint64_t)fad.nFileSizeLow;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return PHX_FALSE;
    const uint64_t size = (uint64_t)fad.nFileSizeHigh << 32 | (uint64_t)fad.nFileSizeLow;
#endif

    *out = size;

    return PHX_TRUE;
}

static PHX_BlockSize PHX_File_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    // TODO
}

static PHX_BlockSize PHX_File_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    // TODO
}

static void PHX_File_Close(PHX_BlockDevice* device)
{
    fclose((FILE*)device->data);
}

PHX_Bool PHX_File_Open(const char* path, PHX_Bool readonly, PHX_BlockDevice* out)
{
    const char* mode = (readonly == PHX_TRUE) ? "r+b" : "rb";

#ifdef _MSC_VER
    FILE* file;
    errno_t fileError = fopen_s(&file, path, mode);
    if (fileError != 0)
#else
    FILE* file = fopen(path, mode);
    if (!file)
#endif
    {
        return PHX_FALSE;
    }

    if (get_file_size(path, &out->blockCount) != PHX_TRUE)
    {
        fclose((FILE*)out->data);
        return PHX_FALSE;
    }

    out->blockSize = 1;
    out->data = (void*)file;

    out->read = PHX_File_Read;
    out->write = PHX_File_Write;
    out->close = PHX_File_Close;

    out->type = "FILE";

    memset(out->name, '\0', sizeof(out->name));
    out->readonly = readonly;

    return PHX_TRUE;
}

PHX_File* PHX_File_Open(const char* path, const char* mode)
{
    PHX_File* file = malloc(sizeof(PHX_File));
    if (!file) return NULL;

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
    uint8_t* buf = buffer;
    const uint64_t start = size;

    while (size > 0)
    {
        const size_t block = (size > SIZE_MAX) ? SIZE_MAX : size;
        const size_t read = fread(buf, 1, block, file->file);
        if (read != block)
            return start - size + read;
        buf += block;
        size -= block;
    }

    return start;
}

uint64_t PHX_File_Write(PHX_File* file, uint64_t size, const void* buffer)
{
    const uint8_t* buf = buffer;
    const uint64_t start = size;

    while (size > 0)
    {
        const size_t block = (size > SIZE_MAX) ? SIZE_MAX : size;
        const size_t written = fwrite(buf, 1, block, file->file);
        if (written != block)
            return start - size + written;
        buf += block;
        size -= block;
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
