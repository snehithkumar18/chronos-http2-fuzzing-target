#ifndef CHRONOS_HUFFMAN_H
#define CHRONOS_HUFFMAN_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

int huffman_decode(const uint8_t* data, size_t size, char* output, size_t output_len);
int huffman_encode(const char* input, size_t input_len, uint8_t* output, size_t output_len);

