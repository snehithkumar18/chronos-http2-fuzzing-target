#ifndef CHRONOS_HPACK_DECODER_H
#define CHRONOS_HPACK_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct {
    char* name;
    char* value;
    size_t name_len;
    size_t value_len;
} hpack_header_t;

typedef struct hpack_context hpack_context_t;

hpack_context_t* hpack_context_create(size_t max_table_size);
void hpack_context_destroy(hpack_context_t* ctx);
int hpack_decode_headers(hpack_context_t* ctx, const uint8_t* data, size_t size, hpack_header_t* headers, int max_headers);
chronos_error_t hpack_decode_integer(const uint8_t* data, size_t size, size_t prefix_bits, uint32_t* result, size_t* bytes_consumed);

