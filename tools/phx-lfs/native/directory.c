#include "directory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define MKDIR(path) _mkdir(path)
#define EXISTS(path) (_access(path, 0) == 0)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define EXISTS(path) (access(path, F_OK) == 0)
#endif

PathType Path_GetType(const char* path)
{
#ifdef _WIN32
    DWORD attrs = GetFileAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return TYPE_OTHER;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) return TYPE_DIR;
    return TYPE_FILE;
#else
    struct stat st;
    if (stat(path, &st) != 0) return TYPE_OTHER;
    if (S_ISDIR(st.st_mode)) return TYPE_DIR;
    if (S_ISREG(st.st_mode)) return TYPE_FILE;
    return TYPE_OTHER;
#endif
}

char **Path_ListDir(const char *dir_path, uint64_t *out_count)
{
    char **list = NULL;
    uint64_t count = 0;

#ifdef _WIN32
    size_t dir_len = strlen(dir_path);
    char *search_path = malloc(dir_len + 3); // for "\\*" and '\0'
    if (!search_path) {
        *out_count = 0;
        return NULL;
    }
    strcpy(search_path, dir_path);
    strcat(search_path, "\\*");

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(search_path, &ffd);
    free(search_path);

    if (hFind == INVALID_HANDLE_VALUE) {
        *out_count = 0;
        return NULL;
    }

    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        size_t name_len = strlen(ffd.cFileName);
        char *full_path = malloc(dir_len + 1 + name_len + 1); // dir + '/' + name + '\0'
        if (!full_path) continue;

        strcpy(full_path, dir_path);
        full_path[dir_len] = '/'; // always use forward slash
        strcpy(full_path + dir_len + 1, ffd.cFileName);

        char **tmp = (char**)realloc((void*)list, sizeof(char*) * (count + 1));
        if (!tmp) {
            free(full_path);
            continue;
        }
        list = tmp;
        list[count++] = full_path;

    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

#else
    DIR *dir = opendir(dir_path);
    if (!dir) {
        *out_count = 0;
        return NULL;
    }

    size_t dir_len = strlen(dir_path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        size_t name_len = strlen(entry->d_name);
        char *full_path = malloc(dir_len + 1 + name_len + 1); // dir + '/' + name + '\0'
        if (!full_path) continue;

        strcpy(full_path, dir_path);
        full_path[dir_len] = '/';
        strcpy(full_path + dir_len + 1, entry->d_name);

        char **tmp = realloc(list, sizeof(char*) * (count + 1));
        if (!tmp) {
            free(full_path);
            continue;
        }
        list = tmp;
        list[count++] = full_path;
    }

    closedir(dir);
#endif

    if (!list) list = (char**)malloc(8);

    *out_count = count;
    return list;
}

int Path_MakeDirsForPath(const char* path, int is_dir)
{
    if (!path || !*path) return 1;

    char *current_path = malloc(1);
    if (!current_path) return 1;
    current_path[0] = '\0';
    size_t current_len = 0;

    const char *p = path;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t segment_len;
        if (slash) segment_len = (size_t)(slash - p);
        else segment_len = is_dir ? strlen(p) : 0;
        
        if (segment_len == 0) break;

        char *tmp = realloc(current_path, current_len + segment_len + 2);
        if (!tmp) {
            free(current_path);
            return -1;
        }
        current_path = tmp;

        if (current_len > 0) current_path[current_len++] = '/';
        memcpy(current_path + current_len, p, segment_len);
        current_len += segment_len;
        current_path[current_len] = '\0';

        if (!EXISTS(current_path)) {
            if (MKDIR(current_path) != 0) {
                free(current_path);
                return 1;
            }
        }

        if (slash) p = slash + 1;
        else break;
    }

    free(current_path);
    return 0;
}
