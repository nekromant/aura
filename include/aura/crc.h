#ifndef CRC_H
#define CRC_H
#include <stdio.h>
#include <stdint.h>

uint8_t crc8(uint8_t crc, uint8_t *data, size_t len);
uint16_t crc16(uint16_t crc, uint8_t const *buffer, size_t len);

#endif
