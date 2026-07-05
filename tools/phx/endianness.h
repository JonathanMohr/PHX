#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t  Endian_Convert_u8_Le(const uint8_t val);
uint8_t  Endian_Convert_u8_Be(const uint8_t val);
uint16_t Endian_Convert_u16_Le(const uint16_t val);
uint16_t Endian_Convert_u16_Be(const uint16_t val);
uint32_t Endian_Convert_u32_Le(const uint32_t val);
uint32_t Endian_Convert_u32_Be(const uint32_t val);
uint64_t Endian_Convert_u64_Le(const uint64_t val);
uint64_t Endian_Convert_u64_Be(const uint64_t val);

#ifdef __cplusplus
}
#endif

