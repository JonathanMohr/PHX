#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   ifndef _XOPEN_SOURCE
#       define _XOPEN_SOURCE 700
#   endif
#   ifndef _FILE_OFFSET_BITS
#       define _FILE_OFFSET_BITS 64
#   endif
#   include <sys/types.h>
#   include <sys/stat.h>
#endif

#include "file.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <base.h>

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

static PHX_Bool PHX_File_GetSize(FILE* file, PHX_BlockSize* out)
{
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END) != 0)
        return PHX_FALSE;
    const int64_t size = _ftelli64(file);
#else
    if (fseeko(file, 0, SEEK_END) != 0)
        return PHX_FALSE;
    const int64_t size = (int64_t)ftello(file);
#endif
    if (size < 0)
        return PHX_FALSE;
    if (PHX_File_Seek(file, 0) != PHX_TRUE)
        return PHX_FALSE;

    *out = (PHX_BlockSize)size;
    return PHX_TRUE;
}


static PHX_BlockSize PHX_File_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    FILE* file = (FILE*)device->data;

    if (PHX_File_Seek(file, block) != PHX_TRUE)
        return 0;

    uint8_t* buf = (uint8_t*)buffer;
    PHX_BlockSize remaining = count;
    while (remaining > 0)
    {
        const size_t toRead = (remaining > SIZE_MAX) ? SIZE_MAX : remaining;
        size_t read = fread(buf, 1, toRead, file);
        if (read == 0)
            break;

        remaining -= read;
        buf += read;
    }

    return count - remaining;
}

static PHX_BlockSize PHX_File_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    if (device->readonly)
        return 0;

    FILE* file = (FILE*)device->data;

    if (PHX_File_Seek(file, block) != PHX_TRUE)
        return 0;

    uint8_t* buf = (uint8_t*)buffer;
    PHX_BlockSize remaining = count;
    while (remaining > 0)
    {
        const size_t toWrite = (remaining > SIZE_MAX) ? SIZE_MAX : remaining;
        size_t written = fwrite(buf, 1, toWrite, file);
        if (written == 0)
            break;

        remaining -= written;
        buf += written;
    }

    return count - remaining;
}

static void PHX_File_Close(PHX_BlockDevice* device)
{
    fclose((FILE*)device->data);
}

PHX_DetailedResult PHX_File_Open(const char* path, PHX_File_Mode mode, PHX_BlockDevice* out, PHX_BlockSize size)
{
    PHX_DetailedResult result = {PHX_SUCCESS, "?"};

    const char* cMode;
    switch (mode)
    {
        case PHX_FILE_MODE_READ:       cMode = "rb";  break;
        case PHX_FILE_MODE_READ_WRITE: cMode = "r+b"; break;
        case PHX_FILE_MODE_CREATE:     cMode = "w+b"; break;
        default:
            result.msg = "Invalid file mode";
            result.code = PHX_ERROR_INTERNAL;
            return result;
    }

    FILE* file;

#ifdef _MSC_VER
    const int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen <= 0)
    {
        result.msg = "Invalid UTF-8 in path";
        result.code = PHX_ERROR_IO;
        return result;
    }

    wchar_t* wpath = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wpath)
    {
        result.msg = "Out of memory";
        result.code = PHX_ERROR_MEMORY;
        return result;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wlen) <= 0)
    {
        free(wpath);
        result.msg = "Invalid UTF-8 in path";
        result.code = PHX_ERROR_IO;
        return result;
    }

    wchar_t wMode[4];
    {
        size_t i = 0;
        for (; cMode[i] && i < 3; i++)
            wMode[i] = (wchar_t)cMode[i];
        wMode[i] = L'\0';
    }

    file = _wfopen(wpath, wMode);
    free(wpath);
#else
    file = fopen(path, cMode);
#endif

    if (!file)
    {
        result.msg = strerror(errno);
        result.code = PHX_ERROR_IO;
        return result;
    }

    PHX_BlockSize blockCount;
    if (mode == PHX_FILE_MODE_CREATE)
    {
        blockCount = size;

        if (size != 0)
        {
            const PHX_Byte zero = 0;
            if (PHX_File_Seek(file, size - 1) != PHX_TRUE ||
                fwrite(&zero, 1, 1, file) != 1 ||
                fflush(file) != 0 ||
                PHX_File_Seek(file, 0) != PHX_TRUE)
            {
                fclose(file);
                result.msg = "Could not preallocate file";
                result.code = PHX_ERROR_IO;
                return result;
            }
        }
    }
    else
    {
        if (PHX_File_GetSize(file, &blockCount) != PHX_TRUE)
        {
            fclose(file);
            result.msg = "Could not get size";
            result.code = PHX_ERROR_IO;
            return result;
        }
    }

    out->blockCount = blockCount;
    out->blockSize = 1;
    out->data = (void*)file;

    out->sectorOffset = 0;

    out->read = PHX_File_Read;
    out->write = PHX_File_Write;
    out->close = PHX_File_Close;

    out->type = "FILE";

    out->readonly = (mode == PHX_FILE_MODE_READ) ? PHX_TRUE : PHX_FALSE;

    return result;
}
