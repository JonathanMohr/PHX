#include "unicode.h"

#include <phx/base/intrinsic.h>

PHX_Bool PHX_Charsets_Unicode_UTF8_IsValid(const char* src, PHX_Size src_len)
{
    const utf8proc_uint8_t* p = (const utf8proc_uint8_t*)src;
    PHX_Size i = 0;

    while (i < src_len)
    {
        if (p[i] < 0x80)
        {
            i++;
            continue;
        }

        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate(p + i, (utf8proc_ssize_t)(src_len - i), &cp);
        if (n < 0)
            return PHX_FALSE;
        i += (PHX_Size)n;
    }
    return PHX_TRUE;
}

PHX_Size PHX_Charsets_Unicode_UTF8_Length(const char* src, PHX_Size max_len)
{
    PHX_Size n = 0;
    while (n < max_len && src[n])
        n++;
    return n;
}


PHX_Bool PHX_Charsets_Unicode_UTF8_IsNormalized(const char* src, PHX_Size src_len, PHX_Charsets_Unicode_Norm_Form form)
{
    const utf8proc_uint8_t* p = (const utf8proc_uint8_t*)src;

    PHX_Size i = 0;
    while (i < src_len && p[i] < 0x80)
        i++;
    if (i == src_len)
        return PHX_TRUE;

    unsigned int opts = UTF8PROC_STABLE;
    switch (form)
    {
        case PHX_CHARSETS_UNICODE_NORM_NFC:  opts |= UTF8PROC_COMPOSE; break;
        case PHX_CHARSETS_UNICODE_NORM_NFD:  opts |= UTF8PROC_DECOMPOSE; break;
        case PHX_CHARSETS_UNICODE_NORM_NFKC: opts |= UTF8PROC_COMPOSE | UTF8PROC_COMPAT; break;
        case PHX_CHARSETS_UNICODE_NORM_NFKD: opts |= UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT; break;
    }

    utf8proc_uint8_t* out = NULL;
    utf8proc_ssize_t n = utf8proc_map(p, (utf8proc_ssize_t)src_len, &out, opts);
    if (n < 0)
        return PHX_FALSE;

    PHX_Bool same = (((PHX_Size)n == src_len) && memcmp(out, p, src_len) == 0) ? PHX_TRUE : PHX_FALSE;
    free(out);
    return same;
}

PHX_Size PHX_Charsets_Unicode_UTF8_Normalize(const char* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Norm_Form form)
{
    unsigned int opts = UTF8PROC_STABLE;
    switch (form)
    {
        case PHX_CHARSETS_UNICODE_NORM_NFC:  opts |= UTF8PROC_COMPOSE; break;
        case PHX_CHARSETS_UNICODE_NORM_NFD:  opts |= UTF8PROC_DECOMPOSE; break;
        case PHX_CHARSETS_UNICODE_NORM_NFKC: opts |= UTF8PROC_COMPOSE | UTF8PROC_COMPAT; break;
        case PHX_CHARSETS_UNICODE_NORM_NFKD: opts |= UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT; break;
    }

    utf8proc_uint8_t* out = NULL;
    utf8proc_ssize_t n = utf8proc_map((const utf8proc_uint8_t*)src, (utf8proc_ssize_t)src_len, &out, opts);
    if (n < 0)
        return 0;

    if (dst != NULL && (PHX_Size)n <= max_dst_len)
        memcpy(dst, out, (PHX_Size)n);

    free(out);
    return (PHX_Size)n;
}


int PHX_Charsets_Unicode_UTF8_CompareCaseSensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len)
{
    const utf8proc_uint8_t* p1 = (const utf8proc_uint8_t*)s1;
    const utf8proc_uint8_t* p2 = (const utf8proc_uint8_t*)s2;

    if (s1_len == s2_len && (s1_len == 0 || memcmp(p1, p2, s1_len) == 0))
        return 0;

    PHX_Size i = 0;
    while (i < s1_len && p1[i] < 0x80) i++;
    int ascii1 = (i == s1_len);

    i = 0;
    while (i < s2_len && p2[i] < 0x80) i++;
    int ascii2 = (i == s2_len);

    if (ascii1 && ascii2)
        return PHX_Charsets_Unicode_CompareBytes(p1, s1_len, p2, s2_len);

    unsigned int opts = UTF8PROC_STABLE | UTF8PROC_COMPOSE;

    utf8proc_uint8_t* n1 = NULL;
    utf8proc_uint8_t* n2 = NULL;
    utf8proc_ssize_t l1 = utf8proc_map(p1, (utf8proc_ssize_t)s1_len, &n1, opts);
    utf8proc_ssize_t l2 = utf8proc_map(p2, (utf8proc_ssize_t)s2_len, &n2, opts);

    int result;
    if (l1 < 0 || l2 < 0)
        result = PHX_Charsets_Unicode_CompareBytes(p1, s1_len, p2, s2_len); // TODO
    else
        result = PHX_Charsets_Unicode_CompareBytes(n1, (PHX_Size)l1, n2, (PHX_Size)l2);

    free(n1);
    free(n2);
    return result;
}

int PHX_Charsets_Unicode_UTF8_CompareCaseInsensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len)
{
    const utf8proc_uint8_t* p1 = (const utf8proc_uint8_t*)s1;
    const utf8proc_uint8_t* p2 = (const utf8proc_uint8_t*)s2;

    PHX_Size i = 0;
    while (i < s1_len && p1[i] < 0x80) i++;
    int ascii1 = (i == s1_len);

    i = 0;
    while (i < s2_len && p2[i] < 0x80) i++;
    int ascii2 = (i == s2_len);

    if (ascii1 && ascii2)
    {
        PHX_Size min = s1_len < s2_len ? s1_len : s2_len;
        for (PHX_Size k = 0; k < min; k++)
        {
            unsigned char a = PHX_Charsets_Unicode_AsciiFold(p1[k]);
            unsigned char b = PHX_Charsets_Unicode_AsciiFold(p2[k]);
            if (a != b)
                return (int)a - (int)b;
        }
        return (s1_len > s2_len) - (s1_len < s2_len);
    }

    unsigned int opts = UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_CASEFOLD;

    utf8proc_uint8_t* n1 = NULL;
    utf8proc_uint8_t* n2 = NULL;
    utf8proc_ssize_t l1 = utf8proc_map(p1, (utf8proc_ssize_t)s1_len, &n1, opts);
    utf8proc_ssize_t l2 = utf8proc_map(p2, (utf8proc_ssize_t)s2_len, &n2, opts);

    int result;
    if (l1 < 0 || l2 < 0)
        result = PHX_Charsets_Unicode_CompareBytes(p1, s1_len, p2, s2_len); // TODO
    else
        result = PHX_Charsets_Unicode_CompareBytes(n1, (PHX_Size)l1, n2, (PHX_Size)l2);

    free(n1);
    free(n2);
    return result;
}

int PHX_Charsets_Unicode_UTF8_CompareSimpleCaseInsensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len)
{
    const utf8proc_uint8_t* p1 = (const utf8proc_uint8_t*)s1;
    const utf8proc_uint8_t* p2 = (const utf8proc_uint8_t*)s2;
    PHX_Size i = 0, j = 0;

    while (i < s1_len && j < s2_len)
    {
        utf8proc_int32_t c1, c2;
        utf8proc_ssize_t n1 = utf8proc_iterate(p1 + i, (utf8proc_ssize_t)(s1_len - i), &c1);
        utf8proc_ssize_t n2 = utf8proc_iterate(p2 + j, (utf8proc_ssize_t)(s2_len - j), &c2);

        if (n1 < 0)
        {
            c1 = 0xFFFD;
            n1 = 1;
        }

        if (n2 < 0)
        {
            c2 = 0xFFFD;
            n2 = 1;
        }

        if (c1 < 0x10000) c1 = utf8proc_toupper(c1);
        if (c2 < 0x10000) c2 = utf8proc_toupper(c2);
        if (c1 != c2)
            return (c1 > c2) - (c1 < c2);

        i += (PHX_Size)n1;
        j += (PHX_Size)n2;
    }

    return (i < s1_len) - (j < s2_len);
}


PHX_Size PHX_Charsets_Unicode_UTF8_ToUTF16(const char* src, PHX_Size src_len, PHX_u8* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness)
{
    const utf8proc_uint8_t* p = (const utf8proc_uint8_t*)src;
    int big = PHX_Charsets_Unicode_IsBig(endianness);
    int bom = endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_LITTLE || endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG;

    PHX_Size units = 0;
    PHX_Size o = 0;

    if (bom)
    {
        if (dst != NULL && max_dst_len >= 1)
        {
            PHX_Charsets_Unicode_Put16(dst, 0xFEFF, big);
            o = 1;
        }
        units = 1;
    }

    PHX_Size i = 0;
    while (i < src_len)
    {
        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate(p + i, (utf8proc_ssize_t)(src_len - i), &cp);
        if (n < 0)
        {
            cp = 0xFFFD;
            n = 1;
        }

        PHX_Size need = (cp >= 0x10000) ? 2 : 1;

        if (dst != NULL && units == o && o + need <= max_dst_len)
        {
            if (need == 2)
            {
                PHX_u32 v = (PHX_u32)cp - 0x10000;
                PHX_Charsets_Unicode_Put16(dst + o * 2, 0xD800 + (v >> 10), big);
                PHX_Charsets_Unicode_Put16(dst + (o + 1) * 2, 0xDC00 + (v & 0x3FF), big);
            }
            else
            {
                PHX_Charsets_Unicode_Put16(dst + o * 2, (PHX_u32)cp, big);
            }
            o += need;
        }

        units += need;
        i += (PHX_Size)n;
    }

    return units;
}

PHX_Size PHX_Charsets_Unicode_UTF8_ToUTF32(const char* src, PHX_Size src_len, PHX_u8* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness)
{
    const utf8proc_uint8_t* p = (const utf8proc_uint8_t*)src;
    int big = PHX_Charsets_Unicode_IsBig(endianness);
    int bom = endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_LITTLE || endianness == PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG;

    PHX_Size units = 0;
    PHX_Size o = 0;

    if (bom)
    {
        if (dst != NULL && max_dst_len >= 1)
        {
            PHX_Charsets_Unicode_Put32(dst, 0xFEFF, big);
            o = 1;
        }
        units = 1;
    }

    PHX_Size i = 0;
    while (i < src_len)
    {
        utf8proc_int32_t cp;
        utf8proc_ssize_t n = utf8proc_iterate(p + i, (utf8proc_ssize_t)(src_len - i), &cp);
        if (n < 0)
        {
            cp = 0xFFFD;
            n = 1;
        }

        if (dst != NULL && units == o && o < max_dst_len)
        {
            PHX_Charsets_Unicode_Put32(dst + o * 4, (PHX_u32)cp, big);
            o++;
        }

        units++;
        i += (PHX_Size)n;
    }

    return units;
}
