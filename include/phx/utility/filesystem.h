#ifndef PHX_UTILITY_FILESYSTEM_H
#define PHX_UTILITY_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/interface/filesystem.h>

PHX_Result PHX_Filesystem_GetEntry(PHX_Filesystem_Device* fs, const char* path, PHX_Filesystem_Node* wd, PHX_Filesystem_Node* nodeOut);
PHX_Result PHX_Filesystem_SeparateParent(PHX_Context* context, const char* path, char nameOut[PHX_NAME_LEN + 1], char** parentPathOut);

#ifdef __cplusplus
}
#endif

#endif
