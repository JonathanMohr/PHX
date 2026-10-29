#ifndef LIBS_CHARSETS_UNICODE_UNICODE_H
#define LIBS_CHARSETS_UNICODE_UNICODE_H

#include <phx/base/intrinsic.h>
#include <phx/charsets/unicode.h>

#define UTF8PROC_STATIC 1
#include <utf8proc.h>


static inline unsigned char PHX_Charsets_Unicode_AsciiFold(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}


static inline int PHX_Charsets_Unicode_CompareBytes(const utf8proc_uint8_t* a, PHX_Size a_len, const utf8proc_uint8_t* b, PHX_Size b_len)
{
    PHX_Size min = a_len < b_len ? a_len : b_len;
    int r = min ? memcmp(a, b, min) : 0;
    if (r != 0)
        return r;
    return (a_len > b_len) - (a_len < b_len);
}


static inline void PHX_Charsets_Unicode_Put16(PHX_u8* dst, PHX_u32 v, int big)
{
    if (big)
    {
        dst[0] = (PHX_u8)(v >> 8);
        dst[1] = (PHX_u8)v;
    }
    else
    {
        dst[0] = (PHX_u8)v;
        dst[1] = (PHX_u8)(v >> 8);
    }
}

static inline PHX_u32 PHX_Charsets_Unicode_Get16(const PHX_u8* src, int big)
{
    return big ? (((PHX_u32)src[0] << 8) | src[1])
               : (((PHX_u32)src[1] << 8) | src[0]);
}


static inline void PHX_Charsets_Unicode_Put32(PHX_u8* dst, PHX_u32 v, int big)
{
    if (big)
    {
        dst[0] = (PHX_u8)(v >> 24); dst[1] = (PHX_u8)(v >> 16);
        dst[2] = (PHX_u8)(v >> 8);  dst[3] = (PHX_u8)v;
    }
    else
    {
        dst[0] = (PHX_u8)v;         dst[1] = (PHX_u8)(v >> 8);
        dst[2] = (PHX_u8)(v >> 16); dst[3] = (PHX_u8)(v >> 24);
    }
}

static inline PHX_u32 PHX_Charsets_Unicode_Get32(const PHX_u8* src, int big)
{
    return big ? (((PHX_u32)src[0] << 24) | ((PHX_u32)src[1] << 16) | ((PHX_u32)src[2] << 8) | src[3])
               : (((PHX_u32)src[3] << 24) | ((PHX_u32)src[2] << 16) | ((PHX_u32)src[1] << 8) | src[0]);
}


static inline int PHX_Charsets_Unicode_IsBig(PHX_Charsets_Unicode_Endianness e)
{
    switch (e)
    {
        case PHX_CHARSETS_UNICODE_ENDIANNESS_BIG:
        case PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG:
            return PHX_TRUE;
        case PHX_CHARSETS_UNICODE_ENDIANNESS_NATIVE:
        {
            const PHX_u16 probe = 1;
            return *(const PHX_u8*)&probe == 0;
        }
        default:
            return PHX_FALSE;
    }
}

#endif
