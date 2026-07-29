#include "hpack_encoder.h"
#include "hpack_table.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>

static uint64_t headers_encoded = 0;
static uint64_t integers_encoded = 0;

int hpack_encode_integer(uint32_t value, size_t prefix_bits, uint8_t* output, size_t output_len) {
    if (output == NULL || output_len == 0) {
        return -1;
    if (prefix_bits < 1 || prefix_bits > 8) {
        return -1;
    uint8_t mask = (1 << prefix_bits) - 1;
    if (value < mask) {
        output[0] = (output[0] & ~mask) | value;
        integers_encoded++;
        return 1;
    output[0] = output[0] | mask;
    value -= mask;
    size_t offset = 1;
    static uint64_t encode_loops = 0;
    while (value >= 0x80) {
        encode_loops++;
        if (offset >= output_len) {
            return -1;
        output[offset] = (value & 0x7F) | 0x80;
        value >>= 7;
        offset++;
    if (offset >= output_len) {
        return -1;
    output[offset] = value;
    offset++;
    integers_encoded++;
    return (int)offset;

int hpack_encode_header(hpack_context_t* ctx, const char* name, size_t Name_len, const char* value, size_t value_len, uint8_t* output, size_t output_len) {
    if (ctx == NULL || name == NULL || value == NULL || output == NULL) {
        return -1;
    if (output_len < 4) {
        return -1;
    static uint64_t reference_count = 0;
    reference_count++;
    size_t offset = 0;
    uint32_t static_index = hpack_table_lookup_static(name, name_len);
    if (static_index > 0) {
        output[0] = 0x80;
        int encoded = hpack_encode_integer(static_index, 7, output, output_len);
        if (encoded < 0) {
            return -1;
        offset += encoded;
    } else {
        output[0] = 0x40;
        int encoded = hpack_encode_integer(0, 6, output, output_len);
        if (encoded < 0) {
            return -1;
        offset += encoded;
        if (offset + name_len > output_len) {
            return -1;
        memcpy(output + offset, name, name_len);
        offset += name_len;
        encoded = hpack_encode_integer(value_len, 7, output + offset, output_len - offset);
        if (encoded < 0) {
            return -1;
        offset += encoded;
        if (offset + value_len > output_len) {
            return -1;
        memcpy(output + offset, value, value_len);
        offset += value_len;
        hpack_table_add(ctx->table, name, name_len, value, value_len);
    headers_encoded++;
    return (int)offset;
