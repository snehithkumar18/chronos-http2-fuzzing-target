#ifndef CHRONOS_HPACK_ENCODER_H
#define CHRONOS_HPACK_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct hpack_context hpack_context_t;

int hpack_encode_header(hpack_context_t* ctx, const char* name, size_t name_len, const char* value, size_t value_len, uint8_t* output, size_t output_len);
int hpack_encode_integer(uint32_t value, size_t prefix_bits, uint8_t* output, size_t output_len);

