#ifndef PHX_TYPES_H
#define PHX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/detail.h>

#if defined(PHX_DETAIL_WINDOWS)

    typedef signed char PHX_i8;
    typedef unsigned char PHX_u8;
    #define PHX_FORMAT_I8 "hhd"
    #define PHX_FORMAT_U8 "hhu"
    #define PHX_FORMAT_O8 "hho"
    #define PHX_FORMAT_X8 "hhX"

    typedef short PHX_i16;
    typedef unsigned short PHX_u16;
    #define PHX_FORMAT_I16 "hd"
    #define PHX_FORMAT_U16 "hu"
    #define PHX_FORMAT_O16 "ho"
    #define PHX_FORMAT_X16 "hX"

    typedef int PHX_i32;
    typedef unsigned int PHX_u32;
    #define PHX_FORMAT_I32 "d"
    #define PHX_FORMAT_U32 "u"
    #define PHX_FORMAT_O32 "o"
    #define PHX_FORMAT_X32 "X"

    typedef long long PHX_i64;
    typedef unsigned long long PHX_u64;
    #define PHX_FORMAT_I64 "lld"
    #define PHX_FORMAT_U64 "llu"
    #define PHX_FORMAT_O64 "llo"
    #define PHX_FORMAT_X64 "llX"

#   if defined(PHX_DETAIL_X86) || defined(PHX_DETAIL_ARM32)

        typedef unsigned long PHX_Size;
        #define PHX_FORMAT_SIZE "lu"
        #define PHX_FORMAT_OSIZE "lo"
        #define PHX_FORMAT_XSIZE "lX"

#   else

        typedef unsigned long long PHX_Size;
        #define PHX_FORMAT_SIZE "llu"
        #define PHX_FORMAT_OSIZE "llo"
        #define PHX_FORMAT_XSIZE "llX"

#   endif

#elif defined(PHX_DETAIL_LINUX)

    typedef signed char PHX_i8;
    typedef unsigned char PHX_u8;
    #define PHX_FORMAT_I8 "hhd"
    #define PHX_FORMAT_U8 "hhu"
    #define PHX_FORMAT_O8 "hho"
    #define PHX_FORMAT_X8 "hhX"

    typedef short PHX_i16;
    typedef unsigned short PHX_u16;
    #define PHX_FORMAT_I16 "hd"
    #define PHX_FORMAT_U16 "hu"
    #define PHX_FORMAT_O16 "ho"
    #define PHX_FORMAT_X16 "hX"

    typedef int PHX_i32;
    typedef unsigned int PHX_u32;
    #define PHX_FORMAT_I32 "d"
    #define PHX_FORMAT_U32 "u"
    #define PHX_FORMAT_O32 "o"
    #define PHX_FORMAT_X32 "X"

#   if defined(PHX_DETAIL_X86) || defined(PHX_DETAIL_ARM32)

        typedef long long PHX_i64;
        typedef unsigned long long PHX_u64;
        #define PHX_FORMAT_I64 "lld"
        #define PHX_FORMAT_U64 "llu"
        #define PHX_FORMAT_O64 "llo"
        #define PHX_FORMAT_X64 "llX"

#   else

        typedef long PHX_i64;
        typedef unsigned long PHX_u64;
        #define PHX_FORMAT_I64 "ld"
        #define PHX_FORMAT_U64 "lu"
        #define PHX_FORMAT_O64 "lo"
        #define PHX_FORMAT_X64 "lX"

#   endif

    typedef unsigned long PHX_Size;
    #define PHX_FORMAT_SIZE "lu"
    #define PHX_FORMAT_OSIZE "lo"
    #define PHX_FORMAT_XSIZE "lX"

#elif defined(PHX_DETAIL_MACOS)

    typedef signed char PHX_i8;
    typedef unsigned char PHX_u8;
    #define PHX_FORMAT_I8 "hhd"
    #define PHX_FORMAT_U8 "hhu"
    #define PHX_FORMAT_O8 "hho"
    #define PHX_FORMAT_X8 "hhX"

    typedef short PHX_i16;
    typedef unsigned short PHX_u16;
    #define PHX_FORMAT_I16 "hd"
    #define PHX_FORMAT_U16 "hu"
    #define PHX_FORMAT_O16 "ho"
    #define PHX_FORMAT_X16 "hX"

    typedef int PHX_i32;
    typedef unsigned int PHX_u32;
    #define PHX_FORMAT_I32 "d"
    #define PHX_FORMAT_U32 "u"
    #define PHX_FORMAT_O32 "o"
    #define PHX_FORMAT_X32 "X"

    typedef long long PHX_i64;
    typedef unsigned long long PHX_u64;
    #define PHX_FORMAT_I64 "lld"
    #define PHX_FORMAT_U64 "llu"
    #define PHX_FORMAT_O64 "llo"
    #define PHX_FORMAT_X64 "llX"

    typedef unsigned long PHX_Size;
    #define PHX_FORMAT_SIZE "lu"
    #define PHX_FORMAT_OSIZE "lo"
    #define PHX_FORMAT_XSIZE "lX"

#endif


#define PHX_U8_MAX 0xFF
#define PHX_U16_MAX 0xFFFF
#define PHX_U32_MAX 0xFFFFFFFF
#define PHX_U64_MAX 0xFFFFFFFFFFFFFFFF


#define PHX_NAME_LEN 512


typedef PHX_u64 PHX_SectorSize;
#define PHX_SECTORSIZE_MAX PHX_U64_MAX
#define PHX_FORMAT_SECTORSIZE PHX_FORMAT_U64
#define PHX_FORMAT_OSECTORSIZE PHX_FORMAT_O64
#define PHX_FORMAT_XSECTORSIZE PHX_FORMAT_X64

typedef PHX_u64 PHX_PartitionSize;
#define PHX_PARTITIONSIZE_MAX PHX_U64_MAX
#define PHX_FORMAT_PARTITIONSIZE PHX_FORMAT_U64
#define PHX_FORMAT_OPARTITIONSIZE PHX_FORMAT_O64
#define PHX_FORMAT_XPARTITIONSIZE PHX_FORMAT_X64

typedef PHX_u64 PHX_FileSize;
#define PHX_FILESIZE_MAX PHX_U64_MAX
#define PHX_FORMAT_FILESIZE PHX_FORMAT_U64
#define PHX_FORMAT_OFILESIZE PHX_FORMAT_O64
#define PHX_FORMAT_XFILESIZE PHX_FORMAT_X64

typedef PHX_u64 PHX_AnySize;
#define PHX_ANYSIZE_MAX PHX_U64_MAX
#define PHX_FORMAT_ANYSIZE PHX_FORMAT_U64
#define PHX_FORMAT_OANYSIZE PHX_FORMAT_O64
#define PHX_FORMAT_XANYSIZE PHX_FORMAT_X64


typedef PHX_i64 PHX_Time;


typedef unsigned char PHX_Bool;
#define PHX_TRUE 1
#define PHX_FALSE 0


#if defined(PHX_DETAIL_CPP11) || defined(PHX_DETAIL_C32)

#   define PHX_NULL nullptr

#elif defined(PHX_DETAIL_CPP) && (defined(PHX_DETAIL_GCC) || defined(PHX_DETAIL_CLANG))

#   define PHX_NULL __null

#elif defined(PHX_DETAIL_CPP)

#   define PHX_NULL 0

#elif defined(PHX_DETAIL_C)

#   define PHX_NULL ((void*)0)

#endif

PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i8) == 1, "PHX_i8: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i8) == 1, "PHX_u8: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i16) == 2, "PHX_i16: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i16) == 2, "PHX_u16: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i32) == 4, "PHX_i32: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i32) == 4, "PHX_u32: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i64) == 8, "PHX_i64: invalid size");
PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_i64) == 8, "PHX_u64: invalid size");

PHX_DETAIL_STATIC_ASSERT(sizeof(PHX_Size) == sizeof(sizeof(0)), "PHX_Size: invalid size");

#ifdef __cplusplus
}
#endif

#endif
