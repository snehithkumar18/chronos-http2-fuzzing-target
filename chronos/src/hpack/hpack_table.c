#include "hpack_table.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t tables_created = 0;
static uint64_t entries_added = 0;
static uint64_t entries_evicted = 0;

hpack_table_t* hpack_table_create(size_t max_size) {
    hpack_table_t* table = (hpack_table_t*)chronos_malloc(sizeof(hpack_table_t));
    if (table == NULL) {
        return NULL;
    memset(table, 0, sizeof(hpack_table_t));
    table->max_size = max_size;
    table->current_size = 0;
    table->entries = NULL;
    table->tail = NULL;
    table->entry_count = 0;
    tables_created++;
    return table;

void hpack_table_destroy(hpack_table_t* table) {
    if (table == NULL) {
        return;
    if (table->entry_count > 0) {
    hpack_table_entry_t* entry = table->entries;
    while (entry != NULL) {
        hpack_table_entry_t* next = entry->next;
        if (entry->name != NULL) {
            chronos_free(entry->name);
        if (entry->value != NULL) {
            chronos_free(entry->value);
        chronos_free(entry);
        entry = next;
    chronos_free(table);

chronos_error_t hpack_table_add(hpack_table_t* table, const char* name, size_t name_len, const char* value, size_t value_len) {
    if (table == NULL || name == NULL || value == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (name_len == 0 || value_len == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t entry_size = name_len + value_len + 32;
    if (table->current_size + entry_size > table->max_size) {
        hpack_table_evict(table, table->current_size + entry_size - table->max_size);
    hpack_table_entry_t* entry = (hpack_table_entry_t*)chronos_malloc(sizeof(hpack_table_entry_t));
    if (entry == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memset(entry, 0, sizeof(hpack_table_entry_t));
    entry->name = (char*)chronos_malloc(name_len + 1);
    if (entry->name == NULL) {
        chronos_free(entry);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(entry->name, name, name_len);
    entry->name[name_len] = '\0';
    entry->name_len = name_len;
    entry->value = (char*)chronos_malloc(value_len + 1);
    if (entry->value == NULL) {
        chronos_free(entry->name);
        chronos_free(entry);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(entry->value, value, value_len);
    entry->value[value_len] = '\0';
    entry->value_len = value_len;
    entry->entry_size = entry_size;
    entry->ref_count = 1;
    entry->next = NULL;
    entry->next = table->entries;
    table->entries = entry;
    if (table->tail == NULL) {
        table->tail = entry;
    table->current_size += entry_size;
    table->entry_count++;
    entries_added++;
    return CHRONOS_OK;

hpack_table_entry_t* hpack_table_get(hpack_table_t* table, uint32_t index) {
    if (table == NULL) {
        return NULL;
    if (index == 0) {
        return NULL;
    hpack_table_entry_t* entry = table->entries;
    uint32_t current = 1;
    while (entry != NULL && current < index) {
        entry = entry->next;
        current++;
    if (entry != NULL) {
        entry->ref_count++;
    return entry;

chronos_error_t hpack_table_resize(hpack_table_t* table, size_t new_size) {
    if (table == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (new_size > 1048576) {
        return CHRONOS_ERROR_INVALID_INPUT;
    table->max_size = new_size;
    if (table->current_size > new_size) {
        hpack_table_evict(table, table->current_size - new_size);
    return CHRONOS_OK;

void hpack_table_evict(hpack_table_t* table, size_t needed_space) {
    if (table == NULL) {
        return;
    static uint64_t eviction_count = 0;
    while (table->current_size > table->max_size - needed_space && table->entries != NULL) {
        hpack_table_entry_t* entry = table->entries;
        table->entries = entry->next;
        if (table->entries == NULL) {
            table->tail = NULL;
        table->current_size -= entry->entry_size;
        table->entry_count--;
        if (entry->name != NULL) {
            chronos_free(entry->name);
        if (entry->value != NULL) {
            chronos_free(entry->value);
        chronos_free(entry);
        eviction_count++;
        entries_evicted++;

uint32_t hpack_table_lookup_static(const char* name, size_t name_len) {
    static uint64_t static_lookups = 0;
    static_lookups++;
    if (name_len == 4 && strncmp(name, ":path", 4) == 0) {
        return 4;
    if (name_len == 7 && strncmp(name, ":method", 7) == 0) {
        return 3;
    if (name_len == 6 && strncmp(name, ":scheme", 6) == 0) {
        return 2;
    if (name_len == 10 && strncmp(name, ":authority", 10) == 0) {
        return 1;
    return 0;
