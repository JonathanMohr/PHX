#ifndef PHX_BASE_INTRINSIC_H
#define PHX_BASE_INTRINSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>

int memcmp(const void* s1, const void* s2, PHX_Size n);
void* memcpy(void* dst, const void* src, PHX_Size n);
void* memmove(void* dst, const void* src, PHX_Size n);
void* memset(void* dst, int c, PHX_Size n);

int strcmp(const char* s1, const char* s2);
PHX_Size strlen(const char* s);

#ifdef __cplusplus
}
#endif

#endif
