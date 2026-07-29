#ifndef CHRONOS_MEMORY_POOL_H
#define CHRONOS_MEMORY_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct memory_block {
    uint8_t* data;
    size_t size;
    size_t used;
    int ref_count;
    uint32_t block_id;
    bool is_free;
    struct memory_block* next;
    struct memory_block* prev;
    uint64_t allocation_time;
    uint32_t owner_stream_id;
    uint8_t allocation_flags;
} memory_block_t;

typedef struct memory_pool {
    memory_block_t* blocks;
    memory_block_t* free_list;
    memory_block_t* active_list;
    size_t total_blocks;
    size_t free_blocks;
    size_t active_blocks;
    size_t total_allocated;
    size_t total_used;
    pool_state_t state;
    uint32_t next_block_id;
    uint64_t pool_creation_time;
    uint32_t fragmentation_threshold;
    bool enable_coalescing;
    bool enable_defragmentation;
    uint32_t defragmentation_counter;
    uint64_t last_defragmentation_time;
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t coalesce_count;
    uint32_t split_count;
    uint32_t max_block_size;
    uint32_t min_block_size;
    uint8_t pool_id;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
} memory_pool_t;

typedef struct pool_stats {
    uint32_t total_allocations;
    uint32_t total_frees;
    uint32_t total_splits;
    uint32_t total_coalesces;
    uint32_t total_defragmentations;
    uint64_t total_allocation_time;
    uint64_t total_free_time;
    uint32_t peak_blocks_used;
    uint32_t peak_memory_used;
    uint32_t current_fragmentation;
    uint32_t fragmentation_events;
} pool_stats_t;

memory_pool_t* memory_pool_create(size_t block_size, size_t max_blocks, uint8_t pool_id);
void memory_pool_destroy(memory_pool_t* pool);
void* memory_pool_alloc(memory_pool_t* pool, size_t size, uint32_t stream_id);
void memory_pool_free(memory_pool_t* pool, void* ptr);
chronos_error_t memory_pool_defragment(memory_pool_t* pool);
chronos_error_t memory_pool_coalesce(memory_pool_t* pool);
chronos_error_t memory_pool_split_block(memory_pool_t* pool, memory_block_t* block, size_t split_size);
chronos_error_t memory_pool_validate(memory_pool_t* pool);
chronos_error_t memory_pool_get_stats(memory_pool_t* pool, pool_stats_t* stats);
chronos_error_t memory_pool_set_corruption_detection(memory_pool_t* pool, bool enable);
chronos_error_t memory_pool_detect_corruption(memory_pool_t* pool);
chronos_error_t memory_pool_recover_corruption(memory_pool_t* pool);
memory_block_t* memory_pool_find_block(memory_pool_t* pool, void* ptr);
chronos_error_t memory_pool_mark_block_active(memory_pool_t* pool, memory_block_t* block);
chronos_error_t memory_pool_mark_block_free(memory_pool_t* pool, memory_block_t* block);
chronos_error_t memory_pool_update_ref_count(memory_pool_t* pool, memory_block_t* block, int delta);
bool memory_pool_is_block_valid(memory_pool_t* pool, memory_block_t* block);
chronos_error_t memory_pool_compact(memory_pool_t* pool);
chronos_error_t memory_pool_reset(memory_pool_t* pool);

