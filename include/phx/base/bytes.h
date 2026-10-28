#ifndef PHX_BASE_BYTES_H
#define PHX_BASE_BYTES_H

#ifdef __cplusplus
extern "C" {
#endif


#include <phx/types.h>
#include <phx/base/intrinsic.h>


static inline PHX_u8 PHX_Read_u8(const PHX_u8* pointer)
{
    return *pointer;
}

static inline PHX_u16 PHX_Read_u16(const PHX_u8* pointer)
{
    PHX_u16 val;
    memcpy(&val, pointer, sizeof(val));
    return val;
}

static inline PHX_u32 PHX_Read_u32(const PHX_u8* pointer)
{
    PHX_u32 val;
    memcpy(&val, pointer, sizeof(val));
    return val;
}

static inline PHX_u64 PHX_Read_u64(const PHX_u8* pointer)
{
    PHX_u64 val;
    memcpy(&val, pointer, sizeof(val));
    return val;
}


static inline void PHX_Write_u8(PHX_u8* pointer, PHX_u8 val)
{
    memcpy(pointer, &val, sizeof(val));
}

static inline void PHX_Write_u16(PHX_u8* pointer, PHX_u16 val)
{
    memcpy(pointer, &val, sizeof(val));
}

static inline void PHX_Write_u32(PHX_u8* pointer, PHX_u32 val)
{
    memcpy(pointer, &val, sizeof(val));
}

static inline void PHX_Write_u64(PHX_u8* pointer, PHX_u64 val)
{
    memcpy(pointer, &val, sizeof(val));
}


#ifdef __cplusplus
}
#endif

#endif
