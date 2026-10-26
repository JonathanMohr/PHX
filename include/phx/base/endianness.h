#ifndef PHX_BASE_ENDIANNESS_H
#define PHX_BASE_ENDIANNESS_H

#ifdef __cplusplus
extern "C" {
#endif


#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define PHX_INTERNAL_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PHX_INTERNAL_BIG_ENDIAN
#else
#error "Endianness not defined"
#endif


#include <phx/types.h>


static inline PHX_u16 PHX_Internal_Keep16(PHX_u16 val) { return val; }
static inline PHX_u16 PHX_Internal_Swap16(PHX_u16 val)
{
    return (PHX_u16)(val << 8) | (PHX_u16)(val >> 8);
}

static inline PHX_u32 PHX_Internal_Keep32(PHX_u32 val) { return val; }
static inline PHX_u32 PHX_Internal_Swap32(PHX_u32 val)
{
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

static inline PHX_u64 PHX_Internal_Keep64(PHX_u64 val) { return val; }
static inline PHX_u64 PHX_Internal_Swap64(PHX_u64 val)
{
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
}


#ifdef PHX_INTERNAL_LITTLE_ENDIAN

#define PHX_Endianness_Convert_u16_Le(val) PHX_Internal_Keep16(val)
#define PHX_Endianness_Convert_u32_Le(val) PHX_Internal_Keep32(val)
#define PHX_Endianness_Convert_u64_Le(val) PHX_Internal_Keep64(val)

#define PHX_Endianness_Convert_u16_Be(val) PHX_Internal_Swap16(val)
#define PHX_Endianness_Convert_u32_Be(val) PHX_Internal_Swap32(val)
#define PHX_Endianness_Convert_u64_Be(val) PHX_Internal_Swap64(val)

#else

#define PHX_Endianness_Convert_u16_Le(val) PHX_Internal_Swap16(val)
#define PHX_Endianness_Convert_u32_Le(val) PHX_Internal_Swap32(val)
#define PHX_Endianness_Convert_u64_Le(val) PHX_Internal_Swap64(val)

#define PHX_Endianness_Convert_u16_Be(val) PHX_Internal_Keep16(val)
#define PHX_Endianness_Convert_u32_Be(val) PHX_Internal_Keep32(val)
#define PHX_Endianness_Convert_u64_Be(val) PHX_Internal_Keep64(val)

#endif


#define Endian_Convert_u16_Le PHX_Endianness_Convert_u16_Le
#define Endian_Convert_u32_Le PHX_Endianness_Convert_u32_Le
#define Endian_Convert_u64_Le PHX_Endianness_Convert_u64_Le

#define Endian_Convert_u16_Be PHX_Endianness_Convert_u16_Be
#define Endian_Convert_u32_Be PHX_Endianness_Convert_u32_Be
#define Endian_Convert_u64_Be PHX_Endianness_Convert_u64_Be


#ifdef __cplusplus
}
#endif

#endif
