#define _CRT_SECURE_NO_WARNINGS

#include <phx/native/filesystem.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <stdlib.h>
#else
#   ifndef _XOPEN_SOURCE
#       define _XOPEN_SOURCE 700
#   endif
#   ifndef _FILE_OFFSET_BITS
#       define _FILE_OFFSET_BITS 64
#   endif
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <errno.h>
#   include <string.h>
#endif

#include <stdio.h>

static PHX_File_Operations PHX_Native_File_Operations = {
    PHX_Native_File_Read,
    PHX_Native_File_Write,
    PHX_Native_File_Close
};


static inline PHX_Bool PHX_File_Seek(FILE* file, PHX_FileSize pos)
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

static PHX_Bool PHX_File_GetSize(FILE* file, PHX_FileSize* out)
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

    *out = (PHX_FileSize)size;
    return PHX_TRUE;
}


PHX_DetailedResult PHX_Native_File_Open(const char* path, PHX_Native_File_Mode mode, PHX_File_Device* out, PHX_FileSize size)
{
    PHX_DetailedResult result = {PHX_SUCCESS, "?"};

    const char* cMode;
    switch (mode)
    {
        case PHX_NATIVE_FILE_MODE_READ:       cMode = "rb";  break;
        case PHX_NATIVE_FILE_MODE_READ_WRITE: cMode = "r+b"; break;
        case PHX_NATIVE_FILE_MODE_CREATE:     cMode = "w+b"; break;
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

    PHX_FileSize fileSize;
    if (mode == PHX_NATIVE_FILE_MODE_CREATE)
    {
        fileSize = size;

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
        if (PHX_File_GetSize(file, &fileSize) != PHX_TRUE)
        {
            fclose(file);
            result.msg = "Could not get size";
            result.code = PHX_ERROR_IO;
            return result;
        }
    }

    out->fileSize = fileSize;
    out->ops = &PHX_Native_File_Operations;
    out->file = (void*)file;
    out->readonly = (mode == PHX_NATIVE_FILE_MODE_READ) ? PHX_TRUE : PHX_FALSE;

    return result;
}

void PHX_Native_File_Close(PHX_File_Device* device)
{
    fclose((FILE*)device->file);
}



PHX_FileSize PHX_Native_File_Read(PHX_File_Device* device, void* buffer, PHX_FileSize offset, PHX_FileSize count)
{
    FILE* file = (FILE*)device->file;

    if (PHX_File_Seek(file, offset) != PHX_TRUE)
        return 0;

    uint8_t* buf = (uint8_t*)buffer;
    PHX_FileSize remaining = count;
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

PHX_FileSize PHX_Native_File_Write(PHX_File_Device* device, const void* buffer, PHX_FileSize offset, PHX_FileSize count)
{
    if (device->readonly == PHX_TRUE)
        return 0;

    FILE* file = (FILE*)device->file;

    if (PHX_File_Seek(file, offset) != PHX_TRUE)
        return 0;

    uint8_t* buf = (uint8_t*)buffer;
    PHX_FileSize remaining = count;
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
