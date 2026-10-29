#include "unicode.h"

#include <phx/base/intrinsic.h>

PHX_Bool PHX_Charsets_Unicode_UTF16_IsValid(const PHX_u8* src, PHX_Size src_len, PHX_Charsets_Unicode_Endianness endianness)
{
    int big = PHX_Charsets_Unicode_IsBig(endianness);

    PHX_Size i = 0;
    while (i < src_len)
    {
        PHX_u32 u = PHX_Charsets_Unicode_Get16(src + i * 2, big);

        if (u >= 0xD800 && u <= 0xDBFF)
        {
            if (i + 1 >= src_len)
                return PHX_FALSE;
            PHX_u32 lo = PHX_Charsets_Unicode_Get16(src + (i + 1) * 2, big);
            if (lo < 0xDC00 || lo > 0xDFFF)
                return PHX_FALSE;
            i += 2;
        }
        else if (u >= 0xDC00 && u <= 0xDFFF)
            return PHX_FALSE;
        else
            i++;
    }

    return PHX_TRUE;
}

PHX_Size PHX_Charsets_Unicode_UTF16_Length(const PHX_u8* src, PHX_Size max_len)
{
    PHX_Size n = 0;
    while (n < max_len && PHX_Charsets_Unicode_Get16(src + n * 2, 0) != 0)
        n++;
    return n;
}


PHX_Size PHX_Charsets_Unicode_UTF16_ToUTF8(const PHX_u8* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness)
{
    int big = PHX_Charsets_Unicode_IsBig(endianness);
    int bom = endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_LITTLE || endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG;

    PHX_Size start = 0;
    if (bom && src_len > 0 && PHX_Charsets_Unicode_Get16(src, big) == 0xFEFF)
        start = 1;

    PHX_Size bytes = 0;
    PHX_Size o = 0;
    PHX_Size i = start;

    while (i < src_len)
    {
        PHX_u32 u = PHX_Charsets_Unicode_Get16(src + i * 2, big);
        utf8proc_int32_t cp;

        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < src_len)
        {
            PHX_u32 lo = PHX_Charsets_Unicode_Get16(src + (i + 1) * 2, big);
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                cp = (utf8proc_int32_t)(0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00));
                i += 2;
            }
            else
            {
                cp = 0xFFFD;
                i++;
            }
        }
        else if (u >= 0xD800 && u <= 0xDFFF)
        {
            cp = 0xFFFD;
            i++;
        }
        else
        {
            cp = (utf8proc_int32_t)u;
            i++;
        }

        PHX_Size len = (cp < 0x80) ? 1 : (cp < 0x800) ? 2 : (cp < 0x10000) ? 3 : 4;

        if (dst != NULL && bytes == o && o + len <= max_dst_len)
        {
            utf8proc_encode_char(cp, (utf8proc_uint8_t*)dst + o);
            o += len;
        }

        bytes += len;
    }

    return bytes;
}
