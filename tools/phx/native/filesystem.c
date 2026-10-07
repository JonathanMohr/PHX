#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN

#   include <stdlib.h>

#   include <windows.h>

typedef struct PHX_WinDir
{
    HANDLE handle;
    WIN32_FIND_DATAW data;
    int hasPending;
} PHX_WinDir;

#else
#   ifndef _DEFAULT_SOURCE
#       define _DEFAULT_SOURCE
#   endif
#   ifndef _DARWIN_C_SOURCE
#       define _DARWIN_C_SOURCE
#   endif
#   ifndef _POSIX_C_SOURCE
#       define _POSIX_C_SOURCE 200809L
#   endif

#   include <stdlib.h>
#   include <string.h>

#   include <dirent.h>
#   include <errno.h>
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif

#include "filesystem.h"

PHX_Bool PHX_Native_GetPathType(const char* path, PHX_Native_Type* typeOut)
{
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen <= 0)
        return PHX_FALSE;

    wchar_t* wpath = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wpath)
        return PHX_FALSE;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wlen) <= 0)
    {
        free(wpath);
        return PHX_FALSE;
    }

    DWORD attr = GetFileAttributesW(wpath);
    free(wpath);

    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        *typeOut = PHX_NATIVE_OTHER;
        return PHX_FALSE;
    }

    *typeOut = (attr & FILE_ATTRIBUTE_DIRECTORY) ? PHX_NATIVE_DIRECTORY : PHX_NATIVE_FILE;
    return PHX_TRUE;
#else
    struct stat st;
    if (stat(path, &st) != 0)
    {
        *typeOut = PHX_NATIVE_OTHER;
        return PHX_FALSE;
    }

    if (S_ISDIR(st.st_mode))      *typeOut = PHX_NATIVE_DIRECTORY;
    else if (S_ISREG(st.st_mode)) *typeOut = PHX_NATIVE_FILE;
    else                          *typeOut = PHX_NATIVE_OTHER;
    return PHX_TRUE;
#endif
}

PHX_Bool PHX_Native_OpenDir(const char* path, PHX_Native_Directory* dirOut)
{
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen <= 0)
        return PHX_FALSE;

    wchar_t* pattern = (wchar_t*)malloc((size_t)(wlen + 2) * sizeof(wchar_t));
    if (!pattern)
        return PHX_FALSE;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, pattern, wlen) <= 0)
    {
        free(pattern);
        return PHX_FALSE;
    }

    const DWORD attr = GetFileAttributesW(pattern);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        free(pattern);
        return PHX_FALSE;
    }

    size_t n = (size_t)wlen - 1;
    if (n > 0 && pattern[n - 1] != L'\\' && pattern[n - 1] != L'/')
        pattern[n++] = L'\\';
    pattern[n++] = L'*';
    pattern[n]   = L'\0';

    PHX_WinDir* dir = (PHX_WinDir*)malloc(sizeof *dir);
    if (!dir)
    {
        free(pattern);
        return PHX_FALSE;
    }

    dir->handle = FindFirstFileW(pattern, &dir->data);
    free(pattern);

    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        free(dir);
        return PHX_FALSE;
    }

    dir->hasPending = 1;
    *dirOut = dir;
#else
    DIR* openDir = opendir(path);
    if (!openDir)
        return PHX_FALSE;
    *dirOut = openDir;
#endif
    return PHX_TRUE;
}

void PHX_Native_CloseDir(PHX_Native_Directory* dir)
{
#ifdef _WIN32
    PHX_WinDir* d = (PHX_WinDir*)*dir;
    FindClose(d->handle);
    free(d);
#else
    DIR* openDir = *dir;
    closedir(openDir);
#endif
    *dir = PHX_NULL;
}


PHX_Bool PHX_Native_GetEntry(PHX_Native_Directory* dir, PHX_Native_Entry* entryOut)
{
    entryOut->name = PHX_NULL;

#ifdef _WIN32
    PHX_WinDir* d = (PHX_WinDir*)*dir;

    while (1)
    {
        if (d->hasPending)
            d->hasPending = 0;
        else if (!FindNextFileW(d->handle, &d->data))
            return PHX_FALSE;

        const wchar_t* n = d->data.cFileName;
        if (n[0] == L'.' && (n[1] == L'\0' || (n[1] == L'.' && n[2] == L'\0')))
            continue;
        break;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        return PHX_FALSE;

    char* name = (char*)malloc((size_t)len);
    if (!name)
        return PHX_FALSE;

    if (WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, name, len, NULL, NULL) <= 0)
    {
        free(name);
        return PHX_FALSE;
    }

    entryOut->name = name;
    if (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        entryOut->type = PHX_NATIVE_DIRECTORY;
        entryOut->size = 0;
    }
    else
    {
        entryOut->type = PHX_NATIVE_FILE;
        entryOut->size = ((PHX_Size)d->data.nFileSizeHigh << 32) | (PHX_Size)d->data.nFileSizeLow;
    }
#else
    DIR* d = (DIR*)*dir;
    struct dirent* e;

    while (1)
    {
        e = readdir(d);
        if (!e)
            return PHX_FALSE;

        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
            break;
    }

    char* name = strdup(e->d_name);
    if (!name)
        return PHX_FALSE;

    PHX_Native_Type type = PHX_NATIVE_FILE;
    PHX_Size size = 0;
    int isDir = 0;

#   ifdef DT_DIR
    if (e->d_type == DT_DIR)
        isDir = 1;
#   endif

    if (!isDir)
    {
        struct stat st;
        if (fstatat(dirfd(d), name, &st, 0) == 0)
        {
            if (S_ISDIR(st.st_mode))
                isDir = 1;
            else
                size = (PHX_Size)st.st_size;
        }
    }

    if (isDir)
        type = PHX_NATIVE_DIRECTORY;

    entryOut->name = name;
    entryOut->type = type;
    entryOut->size = isDir ? 0 : size;
#endif
    return PHX_TRUE;
}

void PHX_Native_CleanupEntry(PHX_Native_Entry* entry)
{
    free((void*)entry->name);
    entry->name = PHX_NULL;
}


PHX_Bool PHX_Native_MakeDirectory(const char* path)
{
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wlen <= 0)
        return PHX_FALSE;

    wchar_t* wpath = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wpath)
        return PHX_FALSE;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wlen) <= 0)
    {
        free(wpath);
        return PHX_FALSE;
    }

    PHX_Bool result = PHX_FALSE;

    if (CreateDirectoryW(wpath, NULL))
    {
        result = PHX_TRUE;
    }
    else if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        DWORD attr = GetFileAttributesW(wpath);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            result = PHX_TRUE;
    }

    free(wpath);
    return result;
#else
    if (mkdir(path, 0777) == 0)
        return PHX_TRUE;

    if (errno == EEXIST)
    {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return PHX_TRUE;
    }
    return PHX_FALSE;
#endif
}
