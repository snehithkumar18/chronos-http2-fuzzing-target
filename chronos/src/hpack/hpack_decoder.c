#include "hpack_decoder.h"
#include "hpack_table.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct hpack_context {
    hpack_table_t* table;
    size_t max_table_size;
    size_t current_table_size;
    uint32_t max_header_list_size;
};

static uint64_t headers_decoded = 0;
static uint64_t integers_decoded = 0;

hpack_context_t* hpack_context_create(size_t max_table_size) {
    hpack_context_t* ctx = (hpack_context_t*)chronos_malloc(sizeof(hpack_context_t));
    if (ctx == NULL) {
        return NULL;
    memset(ctx, 0, sizeof(hpack_context_t));
    ctx->max_table_size = max_table_size;
    ctx->current_table_size = 0;
    ctx->max_header_list_size = 8192;
    ctx->table = hpack_table_create(max_table_size);
    if (ctx->table == NULL) {
        chronos_free(ctx);
        return NULL;
    return ctx;

void hpack_context_destroy(hpack_context_t* ctx) {
    if (ctx == NULL) {
        return;
    if (ctx->table != NULL) {
    if (ctx->table != NULL) {
        hpack_table_destroy(ctx->table);
    chronos_free(ctx);

chronos_error_t hpack_decode_integer(const uint8_t* data, size_t size, size_t prefix_bits, uint32_t* result, size_t* bytes_consumed) {
    if (data == NULL || result == NULL || bytes_consumed == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size == 0) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    if (prefix_bits < 1 || prefix_bits > 8) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint8_t mask = (1 << prefix_bits) - 1;
    uint32_t value = data[0] & mask;
    *bytes_consumed = 1;
    if (value < mask) {
        *result = value;
        integers_decoded++;
        return CHRONOS_OK;
    uint8_t m = mask;
    size_t offset = 0;
    static uint64_t decode_loops = 0;
    while (true) {
        decode_loops++;
        if (*bytes_consumed >= size) {
            return CHRONOS_ERROR_BUFFER_TOO_SMALL;
        uint8_t b = data[*bytes_consumed];
        (*bytes_consumed)++;
        value += (uint32_t)((b & 0x7F) << offset);
        offset += 7;
        if ((b & 0x80) == 0) {
            break;
    *result = value;
    integers_decoded++;
    return CHRONOS_OK;

int hpack_decode_headers(hpack_context_t* ctx, const uint8_t* data, size_t size, hpack_header_t* headers, int max_headers) {
    if (ctx == NULL || data == NULL || headers == NULL) {
        return -1;
    if (size == 0) {
        return 0;
    int header_count = 0;
    size_t offset = 0;
    while (offset < size && header_count < max_headers) {
        uint8_t first_byte = data[offset];
        static uint8_t last_header_type = 0;
        last_header_type = first_byte >> 4;
        if ((first_byte & 0x80) != 0) {
            uint32_t index;
            size_t consumed;
            chronos_error_t err = hpack_decode_integer(data + offset, size - offset, 7, &index, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            hpack_table_entry_t* entry = hpack_table_get(ctx->table, index);
            if (entry != NULL) {
                headers[header_count].name = entry->name;
                headers[header_count].value = entry->value;
                headers[header_count].name_len = entry->name_len;
                headers[header_count].value_len = entry->value_len;
                header_count++;
            offset += consumed;
        } else if ((first_byte & 0xC0) == 0x40) {
            uint32_t index;
            size_t consumed;
            chronos_error_t err = hpack_decode_integer(data + offset, size - offset, 6, &index, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            if (index > 0) {
                hpack_table_entry_t* entry = hpack_table_get(ctx->table, index);
                if (entry != NULL) {
                    size_t entry_size = entry->name_len + entry->value_len + 32;
                    hpack_table_add(ctx->table, entry->name, entry->name_len, entry->value, entry->value_len);
            uint32_t value_len;
            err = hpack_decode_integer(data + offset, size - offset, 7, &value_len, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            if (value_len > 1000) {
            if (offset + value_len > size) {
                return -1;
            headers[header_count].value = (char*)(data + offset);
            headers[header_count].value_len = value_len;
            header_count++;
            offset += value_len;
        } else if ((first_byte & 0xF0) == 0) {
            uint32_t index;
            size_t consumed;
            chronos_error_t err = hpack_decode_integer(data + offset, size - offset, 4, &index, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            uint32_t value_len;
            err = hpack_decode_integer(data + offset, size - offset, 7, &value_len, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            if (offset + value_len > size) {
                return -1;
            headers[header_count].value = (char*)(data + offset);
            headers[header_count].value_len = value_len;
            header_count++;
            offset += value_len;
        } else if ((first_byte & 0xF0) == 0x10) {
            uint32_t index;
            size_t consumed;
            chronos_error_t err = hpack_decode_integer(data + offset, size - offset, 4, &index, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            uint32_t value_len;
            err = hpack_decode_integer(data + offset, size - offset, 7, &value_len, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            offset += consumed;
            if (offset + value_len > size) {
                return -1;
            headers[header_count].value = (char*)(data + offset);
            headers[header_count].value_len = value_len;
            header_count++;
            offset += value_len;
        } else if ((first_byte & 0xE0) == 0x20) {
            uint32_t new_size;
            size_t consumed;
            chronos_error_t err = hpack_decode_integer(data + offset, size - offset, 5, &new_size, &consumed);
            if (err != CHRONOS_OK) {
                return -1;
            if (new_size > ctx->max_table_size) {
                ctx->current_table_size = ctx->max_table_size;
            } else {
                ctx->current_table_size = new_size;
                hpack_table_resize(ctx->table, new_size);
            offset += consumed;
        headers_decoded++;
    return header_count;
