#ifndef PHX_CHARSETS_UNICODE_H
#define PHX_CHARSETS_UNICODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>

typedef enum
{
    PHX_CHARSETS_UNICODE_NORM_NFC,
    PHX_CHARSETS_UNICODE_NORM_NFD,
    PHX_CHARSETS_UNICODE_NORM_NFKC,
    PHX_CHARSETS_UNICODE_NORM_NFKD
} PHX_Charsets_Unicode_Norm_Form;

typedef enum
{
    PHX_CHARSETS_UNICODE_ENDIANNESS_LITTLE,
    PHX_CHARSETS_UNICODE_ENDIANNESS_BIG,
    PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_LITTLE,
    PHX_CHARSETS_UNICODE_ENDIANNESS_BOM_BIG,
    PHX_CHARSETS_UNICODE_ENDIANNESS_NATIVE
} PHX_Charsets_Unicode_Endianness;

PHX_Bool PHX_Charsets_Unicode_UTF8_IsValid(const char* src, PHX_Size src_len);
PHX_Size PHX_Charsets_Unicode_UTF8_Length(const char* src, PHX_Size max_len);

PHX_Bool PHX_Charsets_Unicode_UTF8_IsNormalized(const char* src, PHX_Size src_len, PHX_Charsets_Unicode_Norm_Form form);
PHX_Size PHX_Charsets_Unicode_UTF8_Normalize(const char* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Norm_Form form);

int PHX_Charsets_Unicode_UTF8_CompareCaseSensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len);
int PHX_Charsets_Unicode_UTF8_CompareCaseInsensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len);
int PHX_Charsets_Unicode_UTF8_CompareSimpleCaseInsensitive(const char* s1, PHX_Size s1_len, const char* s2, PHX_Size s2_len);

PHX_Size PHX_Charsets_Unicode_UTF8_ToUTF16(const char* src, PHX_Size src_len, PHX_u8* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness);
PHX_Size PHX_Charsets_Unicode_UTF8_ToUTF32(const char* src, PHX_Size src_len, PHX_u8* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness);


PHX_Bool PHX_Charsets_Unicode_UTF16_IsValid(const PHX_u8* src, PHX_Size src_len, PHX_Charsets_Unicode_Endianness endianness);
PHX_Size PHX_Charsets_Unicode_UTF16_Length(const PHX_u8* src, PHX_Size max_len);

PHX_Size PHX_Charsets_Unicode_UTF16_ToUTF8(const PHX_u8* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness);



PHX_Bool PHX_Charsets_Unicode_UTF32_IsValid(const PHX_u8* src, PHX_Size src_len, PHX_Charsets_Unicode_Endianness endianness);
PHX_Size PHX_Charsets_Unicode_UTF32_Length(const PHX_u8* src, PHX_Size max_len);

PHX_Size PHX_Charsets_Unicode_UTF32_ToUTF8(const PHX_u8* src, PHX_Size src_len, char* dst, PHX_Size max_dst_len, PHX_Charsets_Unicode_Endianness endianness);

#ifdef __cplusplus
}
#endif

#endif
