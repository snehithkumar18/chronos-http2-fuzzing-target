#ifndef CHRONOS_HPACK_TABLE_H
#define CHRONOS_HPACK_TABLE_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct hpack_table_entry {
    char* name;
    char* value;
    size_t name_len;
    size_t value_len;
    size_t entry_size;
    int ref_count;
    struct hpack_table_entry* next;
} hpack_table_entry_t;

typedef struct hpack_table {
    hpack_table_entry_t* entries;
    hpack_table_entry_t* tail;
    size_t max_size;
    size_t current_size;
    uint32_t entry_count;
} hpack_table_t;

hpack_table_t* hpack_table_create(size_t max_size);
void hpack_table_destroy(hpack_table_t* table);
chronos_error_t hpack_table_add(hpack_table_t* table, const char* name, size_t name_len, const char* value, size_t value_len);
hpack_table_entry_t* hpack_table_get(hpack_table_t* table, uint32_t index);
chronos_error_t hpack_table_resize(hpack_table_t* table, size_t new_size);
uint32_t hpack_table_lookup_static(const char* name, size_t name_len);
void hpack_table_evict(hpack_table_t* table, size_t needed_space);

