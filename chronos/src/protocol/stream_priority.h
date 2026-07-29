#ifndef CHRONOS_STREAM_PRIORITY_H
#define CHRONOS_STREAM_PRIORITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct priority_node {
    uint32_t stream_id;
    uint32_t weight;
    uint32_t dependency_id;
    bool exclusive;
    int32_t priority_value;
    uint64_t last_update_time;
    uint32_t update_count;
    struct priority_node* parent;
    struct priority_node* left;
    struct priority_node* right;
    struct priority_node* children;
    struct priority_node* next_sibling;
    struct priority_node* prev_sibling;
    uint32_t depth;
    uint32_t subtree_size;
    bool is_active;
    uint32_t pending_bytes;
    uint32_t sent_bytes;
    double effective_priority;
    uint32_t heap_index;
    uint8_t state_flags;
} priority_node_t;

typedef struct priority_heap {
    priority_node_t** nodes;
    uint32_t size;
    uint32_t capacity;
    priority_node_t* root;
    uint32_t node_count;
    uint32_t max_depth;
    bool is_min_heap;
    bool enable_rebalancing;
    uint32_t rebalance_counter;
    uint64_t last_rebalance_time;
    uint32_t insertion_count;
    uint32_t deletion_count;
    uint32_t update_count;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t max_priority_level;
    uint32_t min_priority_level;
    uint32_t priority_levels[CHRONOS_MAX_PRIORITY_LEVELS];
} priority_heap_t;

typedef struct priority_stats {
    uint32_t total_nodes;
    uint32_t active_nodes;
    uint32_t total_insertions;
    uint32_t total_deletions;
    uint32_t total_updates;
    uint32_t total_rebalances;
    uint32_t max_heap_size;
    uint32_t current_heap_size;
    uint32_t tree_depth;
    uint32_t rebalance_count;
    uint32_t corruption_count;
    uint64_t total_rebalance_time_ms;
} priority_stats_t;

priority_heap_t* priority_heap_create(uint32_t capacity, bool is_min_heap);
void priority_heap_destroy(priority_heap_t* heap);
chronos_error_t priority_heap_insert(priority_heap_t* heap, uint32_t stream_id, uint32_t weight, 
                                      uint32_t dependency_id, bool exclusive);
chronos_error_t priority_heap_remove(priority_heap_t* heap, uint32_t stream_id);
chronos_error_t priority_heap_update(priority_heap_t* heap, uint32_t stream_id, uint32_t new_weight);
chronos_error_t priority_heap_extract_root(priority_heap_t* heap, uint32_t* stream_id);
chronos_error_t priority_heap_get_highest_priority(priority_heap_t* heap, uint32_t* stream_id);
chronos_error_t priority_heap_set_dependency(priority_heap_t* heap, uint32_t stream_id, 
                                            uint32_t dependency_id, bool exclusive);
chronos_error_t priority_heap_rebalance(priority_heap_t* heap);
chronos_error_t priority_heap_validate(priority_heap_t* heap);
chronos_error_t priority_heap_detect_corruption(priority_heap_t* heap);
chronos_error_t priority_heap_recover_corruption(priority_heap_t* heap);
chronos_error_t priority_heap_get_stats(priority_heap_t* heap, priority_stats_t* stats);
chronos_error_t priority_heap_compact(priority_heap_t* heap);
chronos_error_t priority_heap_resize(priority_heap_t* heap, uint32_t new_capacity);
priority_node_t* priority_heap_find_node(priority_heap_t* heap, uint32_t stream_id);
chronos_error_t priority_heap_update_effective_priority(priority_heap_t* heap, priority_node_t* node);
chronos_error_t priority_heap_heapify_up(priority_heap_t* heap, uint32_t index);
chronos_error_t priority_heap_heapify_down(priority_heap_t* heap, uint32_t index);
chronos_error_t priority_heap_swap_nodes(priority_heap_t* heap, uint32_t i, uint32_t j);
chronos_error_t priority_heap_remove_subtree(priority_heap_t* heap, priority_node_t* node);
chronos_error_t priority_heap_calculate_subtree_size(priority_heap_t* heap, priority_node_t* node);
chronos_error_t priority_heap_update_depth(priority_heap_t* heap, priority_node_t* node);
chronos_error_t priority_heap_validate_tree_structure(priority_heap_t* heap);
chronos_error_t priority_heap_validate_heap_property(priority_heap_t* heap);
chronos_error_t priority_heap_detect_cycles(priority_heap_t* heap);

