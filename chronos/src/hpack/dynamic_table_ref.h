#ifndef CHRONOS_DYNAMIC_TABLE_REF_H
#define CHRONOS_DYNAMIC_TABLE_REF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct table_ref_entry {
    char* name;
    char* value;
    size_t name_len;
    size_t value_len;
    size_t entry_size;
    int32_t ref_count;
    uint32_t entry_id;
    uint64_t creation_time;
    uint64_t last_access_time;
    uint32_t access_count;
    bool is_static;
    bool is_evicted;
    bool is_locked;
    uint8_t entry_flags;
    uint32_t hash;
    struct table_ref_entry* next;
    struct table_ref_entry* prev;
    struct table_ref_entry* hash_next;
    uint32_t dependency_id;
    uint32_t dependent_count;
    struct table_ref_entry** dependents;
    uint32_t dependents_capacity;
    uint8_t compression_level;
    uint8_t priority;
    uint32_t ttl;
    uint64_t expiration_time;
    bool is_expired;
    uint32_t version;
    uint32_t parent_id;
    bool has_parent;
} table_ref_entry_t;

typedef struct dynamic_table_ref {
    table_ref_entry_t* entries;
    table_ref_entry_t* tail;
    table_ref_entry_t* lru_head;
    table_ref_entry_t* lru_tail;
    size_t max_size;
    size_t current_size;
    size_t used_size;
    uint32_t entry_count;
    uint32_t max_entries;
    uint32_t next_entry_id;
    uint32_t table_version;
    uint64_t table_creation_time;
    uint64_t last_update_time;
    uint32_t total_insertions;
    uint32_t total_evictions;
    uint32_t total_lookups;
    uint32_t total_hits;
    uint32_t total_misses;
    uint32_t ref_count_updates;
    uint32_t ref_count_overflows;
    uint32_t ref_count_underflows;
    bool enable_lru;
    bool enable_ref_counting;
    bool enable_dependencies;
    bool enable_compression;
    bool enable_ttl;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t hash_table_size;
    table_ref_entry_t** hash_table;
    uint32_t resize_threshold;
    uint32_t compact_threshold;
    uint32_t defrag_counter;
    uint64_t last_defrag_time;
    uint32_t pending_ref_updates;
    uint32_t max_pending_updates;
    uint32_t deadlock_detection_counter;
    bool deadlock_detected;
    uint32_t circular_ref_count;
    uint32_t max_ref_count;
    uint32_t min_ref_count;
    uint32_t ref_count_histogram[32];
    uint8_t table_id;
} dynamic_table_ref_t;

typedef struct table_ref_stats {
    uint32_t total_entries;
    uint32_t active_entries;
    uint32_t evicted_entries;
    uint32_t locked_entries;
    uint32_t expired_entries;
    uint32_t total_ref_count;
    uint32_t average_ref_count;
    uint32_t peak_ref_count;
    uint32_t circular_references;
    uint32_t dependency_depth;
    uint32_t table_hit_rate;
    uint32_t average_entry_size;
    uint32_t table_utilization;
    uint32_t fragmentation_ratio;
    uint32_t compression_ratio;
    uint64_t total_lookup_time_ms;
    uint32_t deadlock_count;
} table_ref_stats_t;

typedef struct ref_update_operation {
    uint32_t entry_id;
    int32_t delta;
    uint64_t timestamp;
    uint8_t operation_type;
    bool is_pending;
    struct ref_update_operation* next;
} ref_update_operation_t;

dynamic_table_ref_t* dynamic_table_ref_create(size_t max_size, uint32_t max_entries, uint8_t table_id);
void dynamic_table_ref_destroy(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_add(dynamic_table_ref_t* table, const char* name, size_t name_len,
                                      const char* value, size_t value_len);
chronos_error_t dynamic_table_ref_get(dynamic_table_ref_t* table, uint32_t index, 
                                      table_ref_entry_t** entry);
chronos_error_t dynamic_table_ref_lookup(dynamic_table_ref_t* table, const char* name, size_t name_len,
                                          const char* value, size_t value_len, table_ref_entry_t** entry);
chronos_error_t dynamic_table_ref_remove(dynamic_table_ref_t* table, uint32_t entry_id);
chronos_error_t dynamic_table_ref_resize(dynamic_table_ref_t* table, size_t new_size);
chronos_error_t dynamic_table_ref_evict(dynamic_table_ref_t* table, size_t needed_space);
chronos_error_t dynamic_table_ref_increment_ref(dynamic_table_ref_t* table, uint32_t entry_id);
chronos_error_t dynamic_table_ref_decrement_ref(dynamic_table_ref_t* table, uint32_t entry_id);
chronos_error_t dynamic_table_ref_add_dependency(dynamic_table_ref_t* table, uint32_t entry_id, 
                                                   uint32_t dependency_id);
chronos_error_t dynamic_table_ref_remove_dependency(dynamic_table_ref_t* table, uint32_t entry_id,
                                                    uint32_t dependency_id);
chronos_error_t dynamic_table_ref_lock_entry(dynamic_table_ref_t* table, uint32_t entry_id);
chronos_error_t dynamic_table_ref_unlock_entry(dynamic_table_ref_t* table, uint32_t entry_id);
chronos_error_t dynamic_table_ref_set_ttl(dynamic_table_ref_t* table, uint32_t entry_id, uint32_t ttl_ms);
chronos_error_t dynamic_table_ref_check_expiration(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_validate(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_detect_corruption(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_recover_corruption(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_detect_deadlock(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_resolve_deadlock(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_compact(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_defragment(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_get_stats(dynamic_table_ref_t* table, table_ref_stats_t* stats);
chronos_error_t dynamic_table_ref_update_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry);
chronos_error_t dynamic_table_ref_remove_from_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry);
chronos_error_t dynamic_table_ref_add_to_lru(dynamic_table_ref_t* table, table_ref_entry_t* entry);
chronos_error_t dynamic_table_ref_calculate_hash(dynamic_table_ref_t* table, const char* name, 
                                                   size_t name_len, uint32_t* hash);
chronos_error_t dynamic_table_ref_insert_hash(dynamic_table_ref_t* table, table_ref_entry_t* entry);
chronos_error_t dynamic_table_ref_remove_hash(dynamic_table_ref_t* table, table_ref_entry_t* entry);
chronos_error_t dynamic_table_ref_lookup_hash(dynamic_table_ref_t* table, uint32_t hash,
                                               const char* name, size_t name_len, table_ref_entry_t** entry);
chronos_error_t dynamic_table_ref_resize_hash_table(dynamic_table_ref_t* table, uint32_t new_size);
chronos_error_t dynamic_table_ref_process_pending_updates(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_queue_ref_update(dynamic_table_ref_t* table, uint32_t entry_id, 
                                                    int32_t delta);
chronos_error_t dynamic_table_ref_detect_circular_refs(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_break_circular_refs(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_validate_ref_counts(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_reset(dynamic_table_ref_t* table);
chronos_error_t dynamic_table_ref_snapshot(dynamic_table_ref_t* table, uint8_t* snapshot, 
                                            size_t* snapshot_len);
chronos_error_t dynamic_table_ref_restore(dynamic_table_ref_t* table, const uint8_t* snapshot, 
                                          size_t snapshot_len);

