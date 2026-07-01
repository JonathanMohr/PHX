#pragma once

#include <stdint.h>

typedef unsigned char PathType;
#define TYPE_OTHER  ((PathType)0)
#define TYPE_FILE   ((PathType)1)
#define TYPE_DIR    ((PathType)2)

PathType Path_GetType(const char* path);
char** Path_ListDir(const char *dir_path, uint64_t *out_count);
int Path_MakeDirsForPath(const char* path, int is_dir);
