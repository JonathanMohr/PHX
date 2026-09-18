#include "file.h"
#include "types.h"

#ifdef _WIN32
#   include <windows.h>
#else
#   define _FILE_OFFSET_BITS 64
#   include <sys/stat.h>
#endif

#include <stdio.h>

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

static inline PHX_Bool PHX_File_Tell(FILE* file, uint64_t* outPos)
{
#ifdef _WIN32
    const int64_t pos = _ftelli64(file);
    if (pos < 0)
        return PHX_FALSE;
    *outPos = (uint64_t)pos;
#else
    const off_t pos = ftello(file);
    if (pos < 0)
        return PHX_FALSE;
    *outPos = (uint64_t)pos;
#endif
    return PHX_TRUE;
}

static inline PHX_Bool PHX_File_Seek(FILE* file, uint64_t pos)
{
    if (pos > INT64_MAX)
        return PHX_FALSE;
#ifdef _WIN32
    if (_fseeki64(file, (int64_t)pos, SEEK_SET) != 0)
        return PHX_FALSE;
#else
    if (fseeko(file, (int64_t)pos, SEEK_SET) != 0)
        return PHX_FALSE;
#endif
    return PHX_TRUE;
}


static PHX_BlockSize PHX_File_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    FILE* file = (FILE*)device->data;

    PHX_BlockSize currentPos;
    if (PHX_File_Tell(file, &currentPos) != PHX_TRUE)
        return 0;

    if (currentPos != block)
    {
        if (PHX_File_Seek(file, block) != PHX_TRUE)
            return 0;
    }

    uint8_t* buf = (uint8_t*)buffer;
    PHX_BlockSize remaining = count;
    while (remaining > 0)
    {
        const size_t toRead = (remaining > SIZE_MAX) ? SIZE_MAX : remaining;
        if (fread(buf, toRead, 1, file) != 1)
            return count - remaining;

        remaining -= toRead;
        buf += toRead;
    }

    return count;
}

static PHX_BlockSize PHX_File_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    if (device->readonly)
        return 0;

    FILE* file = (FILE*)device->data;

    PHX_BlockSize currentPos;
    if (PHX_File_Tell(file, &currentPos) != PHX_TRUE)
        return 0;

    if (currentPos != block)
    {
        if (PHX_File_Seek(file, block) != PHX_TRUE)
            return 0;
    }

    uint8_t* buf = (uint8_t*)buffer;
    PHX_BlockSize remaining = count;
    while (remaining > 0)
    {
        const size_t toWrite = (remaining > SIZE_MAX) ? SIZE_MAX : remaining;
        if (fwrite(buf, toWrite, 1, file) != 1)
            return count - remaining;

        remaining -= toWrite;
        buf += toWrite;
    }

    return count;
}

static void PHX_File_Close(PHX_BlockDevice* device)
{
    fclose((FILE*)device->data);
}

PHX_Bool PHX_File_Open(const char* path, PHX_Bool readonly, PHX_BlockDevice* out, PHX_BlockSize size)
{
    const char* mode;
    if (size != PHX_FILE_SIZE_NONE)
        mode = "w+b";
    else if (readonly)
        mode = "rb";
    else
        mode = "r+b";

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

    if (size == PHX_FILE_SIZE_NONE)
    {
        if (get_file_size(path, &out->blockCount) != PHX_TRUE)
        {
            fclose((FILE*)out->data);
            return PHX_FALSE;
        }
    }
    else
        out->blockCount = size;

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

struct PHX_File
{
    FILE* file;
};
