#pragma once

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t  Endian_Read_u8_Le(const uint8_t* buf);
uint8_t  Endian_Read_u8_Be(const uint8_t* buf);
uint16_t Endian_Read_u16_Le(const uint8_t* buf);
uint16_t Endian_Read_u16_Be(const uint8_t* buf);
uint32_t Endian_Read_u32_Le(const uint8_t* buf);
uint32_t Endian_Read_u32_Be(const uint8_t* buf);
uint64_t Endian_Read_u64_Le(const uint8_t* buf);
uint64_t Endian_Read_u64_Be(const uint8_t* buf);

void Endian_Write_u8_Le(const uint8_t* buf, uint8_t val);
void Endian_Write_u8_Be(const uint8_t* buf, uint8_t val);
void Endian_Write_u16_Le(const uint8_t* buf, uint16_t val);
void Endian_Write_u16_Be(const uint8_t* buf, uint16_t val);
void Endian_Write_u32_Le(const uint8_t* buf, uint32_t val);
void Endian_Write_u32_Be(const uint8_t* buf, uint32_t val);
void Endian_Write_u64_Le(const uint8_t* buf, uint64_t val);
void Endian_Write_u64_Be(const uint8_t* buf, uint64_t val);

#ifdef __cplusplus
}
#endif

