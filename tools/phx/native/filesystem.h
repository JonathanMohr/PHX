#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

typedef void* PHX_Native_Directory;

typedef enum PHX_Native_Type
{
    PHX_NATIVE_FILE,
    PHX_NATIVE_DIRECTORY,
    PHX_NATIVE_OTHER
} PHX_Native_Type;

typedef struct PHX_Native_Entry
{
    const char* name;
    PHX_Native_Type type;
    PHX_Size size;
} PHX_Native_Entry;

PHX_Bool PHX_Native_GetPathType(const char* path, PHX_Native_Type* typeOut);

PHX_Bool PHX_Native_OpenDir(const char* path, PHX_Native_Directory* dirOut);
void PHX_Native_CloseDir(PHX_Native_Directory* dir);

PHX_Bool PHX_Native_GetEntry(PHX_Native_Directory* dir, PHX_Native_Entry* entryOut);
void PHX_Native_CleanupEntry(PHX_Native_Entry* entry);

PHX_Bool PHX_Native_MakeDirectory(const char* path);

#ifdef __cplusplus
}
#endif
