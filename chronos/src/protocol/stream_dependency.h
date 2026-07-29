#ifndef CHRONOS_STREAM_DEPENDENCY_H
#define CHRONOS_STREAM_DEPENDENCY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../common.h"

typedef struct dependency_node {
    uint32_t stream_id;
    uint32_t parent_id;
    uint32_t weight;
    uint32_t priority;
    uint32_t dependency_count;
    uint32_t dependent_count;
    struct dependency_node** dependents;
    uint32_t dependents_capacity;
    struct dependency_node* parent;
    struct dependency_node* next_sibling;
    struct dependency_node* prev_sibling;
    struct dependency_node* first_child;
    struct dependency_node* last_child;
    uint64_t creation_time;
    uint64_t last_update_time;
    uint32_t update_count;
    uint8_t node_state;
    uint8_t dependency_state;
    bool is_active;
    bool is_blocked;
    bool is_closed;
    bool is_exclusive;
    bool has_circular_dependency;
    uint32_t ref_count;
    uint32_t depth;
    uint32_t subtree_size;
    uint32_t subtree_weight_sum;
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
    uint32_t pending_bytes;
    uint32_t blocked_count;
    uint32_t unblocked_count;
    uint32_t dependency_change_count;
    uint32_t weight_change_count;
    uint8_t flags;
    uint32_t validation_failures;
    uint32_t corruption_count;
    uint64_t last_validation_time;
    uint64_t last_corruption_time;
} dependency_node_t;

typedef struct dependency_tree {
    dependency_node_t* root;
    dependency_node_t* node_hash[1000];
    uint32_t max_nodes;
    uint32_t active_nodes;
    uint32_t total_nodes_created;
    uint32_t total_nodes_destroyed;
    uint32_t default_weight;
    uint32_t default_priority;
    uint32_t max_depth;
    uint32_t max_subtree_size;
    bool enable_exclusive_dependencies;
    bool enable_weight_based_scheduling;
    bool enable_dependency_validation;
    bool enable_cycle_detection;
    bool enable_tree_balancing;
    uint32_t tree_balance_threshold;
    uint32_t validation_interval_ms;
    uint64_t last_validation_time;
    uint64_t tree_creation_time;
    uint64_t last_rebalance_time;
    uint32_t rebalance_count;
    bool is_corrupted;
    uint32_t corruption_detection_counter;
    uint32_t next_node_id;
    uint8_t tree_id;
    uint32_t peak_nodes;
    uint32_t average_nodes;
    uint32_t total_dependency_changes;
    uint32_t total_weight_changes;
    uint32_t circular_dependency_count;
    uint32_t tree_violations;
    uint32_t tree_errors;
    uint32_t tree_warnings;
    uint32_t priority_levels[256];
    uint32_t weight_distribution[256];
    uint32_t depth_distribution[100];
} dependency_tree_t;

typedef struct dependency_stats {
    uint32_t total_nodes;
    uint32_t active_nodes;
    uint32_t blocked_nodes;
    uint32_t closed_nodes;
    uint32_t circular_dependencies;
    uint32_t average_depth;
    uint32_t peak_depth;
    uint32_t average_subtree_size;
    uint32_t peak_subtree_size;
    uint32_t total_dependency_changes;
    uint32_t total_weight_changes;
    uint32_t tree_violations;
    uint32_t tree_errors;
    uint32_t rebalance_count;
    uint64_t total_tree_time_ms;
} dependency_stats_t;

typedef struct dependency_change_result {
    bool success;
    uint8_t result_code;
    char result_message[256];
    uint32_t old_parent_id;
    uint32_t new_parent_id;
    uint32_t stream_id;
    uint32_t change_time_ms;
    bool was_circular;
    bool was_rebalanced;
    bool was_validated;
} dependency_change_result_t;

dependency_tree_t* dependency_tree_create(uint32_t max_nodes, uint32_t default_weight, uint8_t tree_id);
void dependency_tree_destroy(dependency_tree_t* tree);
chronos_error_t dependency_tree_add_node(dependency_tree_t* tree, uint32_t stream_id, uint32_t parent_id,
                                         uint32_t weight, bool exclusive);
chronos_error_t dependency_tree_remove_node(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_change_dependency(dependency_tree_t* tree, uint32_t stream_id,
                                                   uint32_t new_parent_id, bool exclusive,
                                                   dependency_change_result_t* result);
chronos_error_t dependency_tree_change_weight(dependency_tree_t* tree, uint32_t stream_id, uint32_t weight);
chronos_error_t dependency_tree_change_priority(dependency_tree_t* tree, uint32_t stream_id, uint32_t priority);
chronos_error_t dependency_tree_block_node(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_unblock_node(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_close_node(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_find_node(dependency_tree_t* tree, uint32_t stream_id, dependency_node_t** node);
chronos_error_t dependency_tree_find_parent(dependency_tree_t* tree, uint32_t stream_id, dependency_node_t** parent);
chronos_error_t dependency_tree_get_children(dependency_tree_t* tree, uint32_t stream_id,
                                              dependency_node_t** children, uint32_t* count);
chronos_error_t dependency_tree_get_subtree_size(dependency_tree_t* tree, uint32_t stream_id, uint32_t* size);
chronos_error_t dependency_tree_get_depth(dependency_tree_t* tree, uint32_t stream_id, uint32_t* depth);
chronos_error_t dependency_tree_validate(dependency_tree_t* tree);
chronos_error_t dependency_tree_detect_cycles(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_break_cycles(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_rebalance(dependency_tree_t* tree, uint32_t stream_id);
chronos_error_t dependency_tree_rebalance_all(dependency_tree_t* tree);
chronos_error_t dependency_tree_get_stats(dependency_tree_t* tree, dependency_stats_t* stats);
chronos_error_t dependency_tree_cleanup(dependency_tree_t* tree);
chronos_error_t dependency_tree_compact(dependency_tree_t* tree);
chronos_error_t dependency_tree_detect_corruption(dependency_tree_t* tree);
chronos_error_t dependency_tree_recover_corruption(dependency_tree_t* tree);
chronos_error_t dependency_tree_set_max_depth(dependency_tree_t* tree, uint32_t max_depth);
chronos_error_t dependency_tree_set_max_subtree_size(dependency_tree_t* tree, uint32_t max_size);
chronos_error_t dependency_tree_enable_exclusive(dependency_tree_t* tree, bool enable);
chronos_error_t dependency_tree_enable_weight_scheduling(dependency_tree_t* tree, bool enable);
chronos_error_t dependency_tree_add_to_hash(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_remove_from_hash(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_add_child(dependency_tree_t* tree, dependency_node_t* parent, dependency_node_t* child);
chronos_error_t dependency_tree_remove_child(dependency_tree_t* tree, dependency_node_t* parent, dependency_node_t* child);
chronos_error_t dependency_tree_update_subtree_size(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_update_depth(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_increment_ref_count(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_decrement_ref_count(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_calculate_subtree_weight(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_validate_node(dependency_tree_t* tree, dependency_node_t* node);
chronos_error_t dependency_tree_process_closed_nodes(dependency_tree_t* tree);
chronos_error_t dependency_tree_reset(dependency_tree_t* tree);
chronos_error_t dependency_tree_snapshot(dependency_tree_t* tree, uint8_t* snapshot, size_t* snapshot_len);
chronos_error_t dependency_tree_restore(dependency_tree_t* tree, const uint8_t* snapshot, size_t snapshot_len);
chronos_error_t dependency_tree_update_priority_stats(dependency_tree_t* tree, uint8_t priority);

