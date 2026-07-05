#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct PHX_File PHX_File;

PHX_File* PHX_File_Open(const char* path, const char* mode);
void PHX_File_Close(PHX_File* file);

uint64_t PHX_File_Read(PHX_File* file, uint64_t size, void* buffer);
uint64_t PHX_File_Write(PHX_File* file, uint64_t size, const void* buffer);
bool PHX_File_Seek(PHX_File* file, uint64_t offset);
bool PHX_File_Tell(PHX_File* file, uint64_t* pos);

#ifdef __cplusplus
}
#endif
