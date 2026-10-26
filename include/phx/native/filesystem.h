#ifndef PHX_NATIVE_FILESYSTEM_H
#define PHX_NATIVE_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <phx/types.h>
#include <phx/result.h>
#include <phx/device/file.h>


typedef enum PHX_Native_Path_Type
{
    PHX_NATIVE_TYPE_FILE,
    PHX_NATIVE_TYPE_DIRECTORY,
    PHX_NATIVE_TYPE_OTHER
} PHX_Native_Path_Type;

PHX_Bool PHX_Native_Path_GetType(const char* path, PHX_Native_Path_Type* typeOut);


typedef enum PHX_Native_File_Mode
{
    PHX_NATIVE_FILE_MODE_READ,
    PHX_NATIVE_FILE_MODE_READ_WRITE,
    PHX_NATIVE_FILE_MODE_CREATE
} PHX_Native_File_Mode;

/** size is only used when mode is PHX_NATIVE_FILE_MODE_CREATE */
PHX_DetailedResult PHX_Native_File_Open(const char* path, PHX_Native_File_Mode mode, PHX_File_Device* out, PHX_FileSize size);
void PHX_Native_File_Close(PHX_File_Device* device);

PHX_FileSize PHX_Native_File_Read(PHX_File_Device* device, void* buffer, PHX_FileSize offset, PHX_FileSize count);
PHX_FileSize PHX_Native_File_Write(PHX_File_Device* device, const void* buffer, PHX_FileSize offset, PHX_FileSize count);


typedef void* PHX_Native_Directory;

typedef struct PHX_Native_Directory_Entry
{
    const char* name;
    PHX_Native_Path_Type type;
    PHX_Size size;
} PHX_Native_Directory_Entry;

PHX_Bool PHX_Native_Directory_Open(const char* path, PHX_Native_Directory* dirOut);
void PHX_Native_Directory_Close(PHX_Native_Directory* dir);

PHX_Bool PHX_Native_Directory_GetEntry(PHX_Native_Directory* dir, PHX_Native_Directory_Entry* entryOut);
void PHX_Native_Directory_CleanupEntry(PHX_Native_Directory_Entry* entry);

PHX_Bool PHX_Native_Directory_Make(const char* path);


#ifdef __cplusplus
}
#endif

#endif
