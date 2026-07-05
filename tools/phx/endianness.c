#include "endianness.h"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define INTERNAL_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define INTERNAL_BIG_ENDIAN
#else
#error "Endianness not defined"
#endif

uint8_t Endian_Convert_u8_Le(const uint8_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return val;
#elif defined(INTERNAL_BIG_ENDIAN)
    return val;
#endif
}

uint8_t Endian_Convert_u8_Be(const uint8_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return val;
#elif defined(INTERNAL_BIG_ENDIAN)
    return val;
#endif
}

uint16_t Endian_Convert_u16_Le(const uint16_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return val;
#elif defined(INTERNAL_BIG_ENDIAN)
    return (uint16_t)((val << 8) | (val >> 8));
#endif
}

uint16_t Endian_Convert_u16_Be(const uint16_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return (uint16_t)((val << 8) | (val >> 8));
#elif defined(INTERNAL_BIG_ENDIAN)
    return val;
#endif
}

uint32_t Endian_Convert_u32_Le(const uint32_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return val;
#elif defined(INTERNAL_BIG_ENDIAN)
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
#endif
}

uint32_t Endian_Convert_u32_Be(const uint32_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
#elif defined(INTERNAL_BIG_ENDIAN)
    return val;
#endif
}

uint64_t Endian_Convert_u64_Le(const uint64_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return val;
#elif defined(INTERNAL_BIG_ENDIAN)
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
#endif
}

uint64_t Endian_Convert_u64_Be(const uint64_t val)
{
#if defined(INTERNAL_LITTLE_ENDIAN)
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
#elif defined(INTERNAL_BIG_ENDIAN)
    return val;
#endif
}
