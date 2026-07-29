#ifndef CHRONOS_CHECKSUM_H
#define CHRONOS_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

uint32_t chronos_crc32(const uint8_t* data, size_t length);
uint16_t chronos_checksum16(const uint8_t* data, size_t length);

