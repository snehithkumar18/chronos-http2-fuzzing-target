#ifndef CHRONOS_BASE64_H
#define CHRONOS_BASE64_H

#include <stddef.h>
#include <stdint.h>

int chronos_base64_encode(const uint8_t* input, size_t input_len, char* output, size_t output_len);
int chronos_base64_decode(const char* input, size_t input_len, uint8_t* output, size_t output_len);

