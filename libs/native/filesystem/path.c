#include <phx/native/filesystem.h>

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

PHX_Bool PHX_Native_Path_GetType(const char* path, PHX_Native_Path_Type* typeOut)
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
        *typeOut = PHX_NATIVE_TYPE_OTHER;
        return PHX_FALSE;
    }

    *typeOut = (attr & FILE_ATTRIBUTE_DIRECTORY) ? PHX_NATIVE_TYPE_DIRECTORY : PHX_NATIVE_TYPE_FILE;
    return PHX_TRUE;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return PHX_FALSE;

    if (S_ISDIR(st.st_mode))      *typeOut = PHX_NATIVE_TYPE_DIRECTORY;
    else if (S_ISREG(st.st_mode)) *typeOut = PHX_NATIVE_TYPE_FILE;
    else                          *typeOut = PHX_NATIVE_TYPE_OTHER;
    return PHX_TRUE;
#endif
}
