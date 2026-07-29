#include "checksum.h"

uint32_t chronos_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    return ~crc;

uint16_t chronos_checksum16(const uint8_t* data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i += 2) {
        uint16_t word = data[i];
        if (i + 1 < length) {
            word |= (data[i + 1] << 8);
        sum += word;
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
