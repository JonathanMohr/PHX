#include "unicode.h"

#include <phx/base/intrinsic.h>

PHX_Bool PHX_Charsets_Unicode_UTF32_IsValid(const PHX_u8* src, PHX_Size src_len, PHX_Charsets_Unicode_Endianness endianness)
{
    int big = PHX_Charsets_Unicode_IsBig(endianness);

    for (PHX_Size i = 0; i < src_len; i++)
    {
        PHX_u32 u = PHX_Charsets_Unicode_Get32(src + i * 4, big);

        if (u > 0x10FFFF)
            return PHX_FALSE;
        if (u >= 0xD800 && u <= 0xDFFF)
            return PHX_FALSE;
    }

    return PHX_TRUE;
}

PHX_Size PHX_Charsets_Unicode_UTF32_Length(const PHX_u8* src, PHX_Size max_len)
{
    PHX_Size n = 0;
    while (n < max_len && PHX_Charsets_Unicode_Get32(src + n * 4, 0) != 0)
        n++;
    return n;
}


PHX_Size PHX_Charsets_Unicode_UTF32_ToUTF8(const PHX_u8* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness)
{
    int big = PHX_Charsets_Unicode_IsBig(endianness);
    int bom = endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_LITTLE || endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG;

    PHX_Size start = 0;
    if (bom && src_len > 0 && PHX_Charsets_Unicode_Get32(src, big) == 0xFEFF)
        start = 1;

    PHX_Size bytes = 0;
    PHX_Size o = 0;

    PHX_Size i = start;
    while (i < src_len)
    {
        PHX_u32 u = PHX_Charsets_Unicode_Get32(src + i * 4, big);
        utf8proc_int32_t cp;

        if (u > 0x10FFFF || (u >= 0xD800 && u <= 0xDFFF))
            cp = 0xFFFD;
        else
            cp = (utf8proc_int32_t)u;

        PHX_Size len = (cp < 0x80) ? 1 : (cp < 0x800) ? 2 : (cp < 0x10000) ? 3 : 4;

        if (dst != NULL && bytes == o && o + len <= max_dst_len)
        {
            utf8proc_encode_char(cp, (utf8proc_uint8_t*)dst + o);
            o += len;
        }

        bytes += len;
        i++;
    }

    return bytes;
}
