#ifndef PHX_BASE_SERIALIZATION_H
#define PHX_BASE_SERIALIZATION_H

#ifdef __cplusplus
extern "C" {
#endif


#include <phx/base/bytes.h>
#include <phx/base/endianness.h>


#define PHX_INTERNAL_READ_FUNCTION(bitsize, endianness) \
    static inline PHX_u##bitsize PHX_Read_##endianness##_u##bitsize \
    (const PHX_u8* pointer) \
    { \
        PHX_u##bitsize raw = PHX_Read_u##bitsize(pointer); \
        return PHX_Endianness_Convert_u##bitsize##_##endianness(raw); \
    }

#define PHX_INTERNAL_WRITE_FUNCTION(bitsize, endianness) \
    static inline void PHX_Write_##endianness##_u##bitsize \
    (PHX_u8* pointer, PHX_u##bitsize val) \
    { \
        PHX_u##bitsize raw = PHX_Endianness_Convert_u##bitsize##_##endianness(val); \
        PHX_Write_u##bitsize(pointer, raw); \
    }

PHX_INTERNAL_READ_FUNCTION(8,  Le)
PHX_INTERNAL_READ_FUNCTION(16, Le)
PHX_INTERNAL_READ_FUNCTION(32, Le)
PHX_INTERNAL_READ_FUNCTION(64, Le)

PHX_INTERNAL_WRITE_FUNCTION(8,  Le)
PHX_INTERNAL_WRITE_FUNCTION(16, Le)
PHX_INTERNAL_WRITE_FUNCTION(32, Le)
PHX_INTERNAL_WRITE_FUNCTION(64, Le)

PHX_INTERNAL_READ_FUNCTION(8,  Be)
PHX_INTERNAL_READ_FUNCTION(16, Be)
PHX_INTERNAL_READ_FUNCTION(32, Be)
PHX_INTERNAL_READ_FUNCTION(64, Be)

PHX_INTERNAL_WRITE_FUNCTION(8,  Be)
PHX_INTERNAL_WRITE_FUNCTION(16, Be)
PHX_INTERNAL_WRITE_FUNCTION(32, Be)
PHX_INTERNAL_WRITE_FUNCTION(64, Be)

#ifdef __cplusplus
}
#endif

#endif
