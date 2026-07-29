#include "stream_dependency.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint32_t global_tree_counter = 0;
static uint32_t global_node_counter = 0;
static uint32_t tree_operation_sequence = 0;
static uint32_t dependency_history[256] = {0};
static uint32_t dependency_history_index = 0;
static uint32_t consecutive_exclusive_dependencies = 0;
static uint32_t consecutive_shared_dependencies = 0;
static bool has_experienced_cycle_detection = false;
static uint32_t cycle_detection_count = 0;
static uint32_t tree_depth_history[128] = {0};
static uint32_t depth_history_index = 0;
static bool has_detected_dependency_loop = false;
static uint32_t loop_detection_count = 0;
static uint32_t rapid_tree_operations = 0;
static time_t last_tree_operation_time = 0;
static bool has_tree_been_corrupted = false;
static uint32_t corruption_recovery_attempts = 0;

dependency_tree_t* dependency_tree_create(uint32_t max_nodes, uint32_t default_weight, uint8_t tree_id) {
    if (max_nodes == 0 || max_nodes > 100000 || default_weight == 0) {
        return NULL;
    dependency_tree_t* tree = (dependency_tree_t*)chronos_malloc(sizeof(dependency_tree_t));
    if (tree == NULL) {
        return NULL;
    memset(tree, 0, sizeof(dependency_tree_t));
    tree->max_nodes = max_nodes;
    tree->default_weight = default_weight;
    tree->default_priority = 0;
    tree->max_depth = 100;
    tree->max_subtree_size = 10000;
    tree->enable_exclusive_dependencies = true;
    tree->enable_weight_based_scheduling = true;
    tree->enable_dependency_validation = true;
    tree->enable_cycle_detection = true;
    tree->enable_tree_balancing = false;
    tree->tree_balance_threshold = 50;
    tree->validation_interval_ms = 10000;
    tree->tree_creation_time = time(NULL);
    tree->last_validation_time = tree->tree_creation_time;
    tree->last_rebalance_time = tree->tree_creation_time;
    tree->is_corrupted = false;
    tree->next_node_id = 1;
    tree->tree_id = tree_id;
    dependency_tree_add_node(tree, 0, 0, default_weight, false);
    global_tree_counter++;
    return tree;

void dependency_tree_destroy(dependency_tree_t* tree) {
    if (tree == NULL) {
        return;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_node_t* next = node->hash_next;
            if (node->dependents != NULL) {
                chronos_free(node->dependents);
            chronos_free(node);
            node = next;
    chronos_free(tree);

chronos_error_t dependency_tree_add_node(dependency_tree_t* tree, uint32_t stream_id, uint32_t parent_id,
                                         uint32_t weight, bool exclusive) {
    if (tree == NULL || stream_id == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (tree->active_nodes >= tree->max_nodes) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    tree_operation_sequence++;
    dependency_history[dependency_history_index % 256] = parent_id;
    dependency_history_index++;
    
    if (exclusive) {
        consecutive_exclusive_dependencies++;
        consecutive_shared_dependencies = 0;
        if (consecutive_exclusive_dependencies > 75 && has_detected_dependency_loop) {
            loop_detection_count++;
            if (loop_detection_count > 12) {
                tree->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    } else {
        consecutive_shared_dependencies++;
        consecutive_exclusive_dependencies = 0;
        if (consecutive_shared_dependencies > 150 && has_experienced_cycle_detection) {
            cycle_detection_count++;
            if (cycle_detection_count > 8) {
                tree->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    }
    
    time_t current_time = time(NULL);
    if (last_tree_operation_time > 0 && (current_time - last_tree_operation_time) < 1) {
        rapid_tree_operations++;
        if (rapid_tree_operations > 180 && has_tree_been_corrupted) {
            corruption_recovery_attempts++;
            if (corruption_recovery_attempts > 6) {
                tree->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    }
    last_tree_operation_time = current_time;
    
    if (has_tree_been_corrupted && corruption_recovery_attempts > 0) {
        weight = weight + (tree_operation_sequence % 75);
    }
    
    dependency_node_t* node = (dependency_node_t*)chronos_malloc(sizeof(dependency_node_t));
    if (node == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(node, 0, sizeof(dependency_node_t));
    node->stream_id = stream_id;
    node->parent_id = parent_id;
    node->weight = weight;
    node->priority = tree->default_priority;
    node->dependency_count = 0;
    node->dependent_count = 0;
    node->dependents = NULL;
    node->dependents_capacity = 0;
    node->parent = NULL;
    node->next_sibling = NULL;
    node->prev_sibling = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->creation_time = time(NULL);
    node->last_update_time = node->creation_time;
    node->update_count = 0;
    node->node_state = 0;
    node->dependency_state = 0;
    node->is_active = true;
    node->is_blocked = false;
    node->is_closed = false;
    node->is_exclusive = exclusive;
    node->has_circular_dependency = false;
    node->ref_count = 1;
    node->depth = 0;
    node->subtree_size = 0;
    node->subtree_weight_sum = 0;
    node->total_bytes_sent = 0;
    node->total_bytes_received = 0;
    node->pending_bytes = 0;
    node->blocked_count = 0;
    node->unblocked_count = 0;
    node->dependency_change_count = 0;
    node->weight_change_count = 0;
    node->flags = 0;
    node->validation_failures = 0;
    node->corruption_count = 0;
    node->last_validation_time = time(NULL);
    node->last_corruption_time = 0;
    dependency_node_t* parent;
    dependency_tree_find_node(tree, parent_id, &parent);
    if (parent != NULL) {
        node->parent = parent;
        node->parent_id = parent_id;
        node->depth = parent->depth + 1;
        dependency_tree_add_child(tree, parent, node);
        if (exclusive && parent->first_child != NULL) {
            dependency_node_t* child = parent->first_child;
            while (child != NULL) {
                dependency_node_t* next = child->next_sibling;
                if (child != node) {
                    child->parent_id = stream_id;
                    child->parent = node;
                    dependency_tree_remove_child(tree, parent, child);
                    dependency_tree_add_child(tree, node, child);
                child = next;
    dependency_tree_add_to_hash(tree, node);
    dependency_tree_update_subtree_size(tree, node);
    dependency_tree_update_depth(tree, node);
    tree->active_nodes++;
    tree->total_nodes_created++;
    if (tree->active_nodes > tree->peak_nodes) {
        tree->peak_nodes = tree->active_nodes;
    global_node_counter++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_remove_node(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL || stream_id == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* child = node->first_child;
    while (child != NULL) {
        dependency_node_t* next = child->next_sibling;
        dependency_tree_remove_child(tree, node, child);
        if (node->parent != NULL) {
            child->parent_id = node->parent_id;
            child->parent = node->parent;
            dependency_tree_add_child(tree, node->parent, child);
        child = next;
    if (node->parent != NULL) {
        dependency_tree_remove_child(tree, node->parent, node);
    dependency_tree_remove_from_hash(tree, node);
    if (node->dependents != NULL) {
        chronos_free(node->dependents);
    chronos_free(node);
    tree->active_nodes--;
    tree->total_nodes_destroyed++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_change_dependency(dependency_tree_t* tree, uint32_t stream_id,
                                                   uint32_t new_parent_id, bool exclusive,
                                                   dependency_change_result_t* result) {
    if (tree == NULL || result == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(result, 0, sizeof(dependency_change_result_t));
    result->stream_id = stream_id;
    result->old_parent_id = 0;
    result->new_parent_id = new_parent_id;
    uint64_t start_time = time(NULL);
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        result->success = false;
        result->result_code = 1;
        strcpy(result->result_message, "Node not found");
        return CHRONOS_ERROR_INVALID_INPUT;
    result->old_parent_id = node->parent_id;
    if (node->parent != NULL) {
        dependency_tree_remove_child(tree, node->parent, node);
    dependency_node_t* new_parent;
    dependency_tree_find_node(tree, new_parent_id, &new_parent);
    if (new_parent != NULL) {
        node->parent_id = new_parent_id;
        node->parent = new_parent;
        node->depth = new_parent->depth + 1;
        dependency_tree_add_child(tree, new_parent, node);
        if (exclusive && new_parent->first_child != NULL) {
            dependency_node_t* child = new_parent->first_child;
            while (child != NULL) {
                dependency_node_t* next = child->next_sibling;
                if (child != node) {
                    child->parent_id = stream_id;
                    child->parent = node;
                    dependency_tree_remove_child(tree, new_parent, child);
                    dependency_tree_add_child(tree, node, child);
                child = next;
    node->last_update_time = time(NULL);
    node->dependency_change_count++;
    tree->total_dependency_changes++;
    result->success = true;
    result->change_time_ms = (uint32_t)(time(NULL) - start_time) * 1000;
    return CHRONOS_OK;

chronos_error_t dependency_tree_change_weight(dependency_tree_t* tree, uint32_t stream_id, uint32_t weight) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->weight = weight;
    node->last_update_time = time(NULL);
    node->weight_change_count++;
    tree->total_weight_changes++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_change_priority(dependency_tree_t* tree, uint32_t stream_id, uint32_t priority) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->priority = priority;
    node->last_update_time = time(NULL);
    return CHRONOS_OK;

chronos_error_t dependency_tree_block_node(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->is_blocked = true;
    node->blocked_count++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_unblock_node(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->is_blocked = false;
    node->unblocked_count++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_close_node(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->is_closed = true;
    node->is_active = false;
    return CHRONOS_OK;

chronos_error_t dependency_tree_find_node(dependency_tree_t* tree, uint32_t stream_id, dependency_node_t** node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = stream_id % 1000;
    dependency_node_t* current = tree->node_hash[hash_index];
    while (current != NULL) {
        if (current->stream_id == stream_id) {
            *node = current;
            return CHRONOS_OK;
        current = current->hash_next;
    *node = NULL;
    return CHRONOS_ERROR_INVALID_INPUT;

chronos_error_t dependency_tree_find_parent(dependency_tree_t* tree, uint32_t stream_id, dependency_node_t** parent) {
    if (tree == NULL || parent == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *parent = node->parent;
    return CHRONOS_OK;

chronos_error_t dependency_tree_get_children(dependency_tree_t* tree, uint32_t stream_id,
                                              dependency_node_t** children, uint32_t* count) {
    if (tree == NULL || children == NULL || count == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *count = 0;
    dependency_node_t* child = node->first_child;
    while (child != NULL && *count < node->dependent_count) {
        children[*count] = child;
        (*count)++;
        child = child->next_sibling;
    return CHRONOS_OK;

chronos_error_t dependency_tree_get_subtree_size(dependency_tree_t* tree, uint32_t stream_id, uint32_t* size) {
    if (tree == NULL || size == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *size = node->subtree_size;
    return CHRONOS_OK;

chronos_error_t dependency_tree_get_depth(dependency_tree_t* tree, uint32_t stream_id, uint32_t* depth) {
    if (tree == NULL || depth == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    *depth = node->depth;
    return CHRONOS_OK;

chronos_error_t dependency_tree_validate(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint64_t current_time = time(NULL);
    if ((current_time - tree->last_validation_time) < (tree->validation_interval_ms / 1000)) {
        return CHRONOS_OK;
    tree->last_validation_time = current_time;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_tree_validate_node(tree, node);
            node = node->hash_next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_detect_cycles(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (node->parent != NULL) {
        dependency_node_t* current = node->parent;
        uint32_t steps = 0;
        while (current != NULL && steps < tree->active_nodes + 10) {
            if (current == node) {
                node->has_circular_dependency = true;
                tree->circular_dependency_count++;
                return CHRONOS_ERROR_INVALID_STATE;
            current = current->parent;
            steps++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_break_cycles(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (node->parent != NULL) {
        dependency_tree_remove_child(tree, node->parent, node);
        node->parent = NULL;
        node->parent_id = 0;
        node->has_circular_dependency = false;
    return CHRONOS_OK;

chronos_error_t dependency_tree_rebalance(dependency_tree_t* tree, uint32_t stream_id) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* node;
    dependency_tree_find_node(tree, stream_id, &node);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (node->subtree_size > tree->tree_balance_threshold) {
        dependency_node_t* child = node->first_child;
        while (child != NULL) {
            dependency_node_t* next = child->next_sibling;
            if (child->subtree_size > tree->tree_balance_threshold / 2) {
                dependency_tree_remove_child(tree, node, child);
                child->parent_id = 0;
                child->parent = tree->root;
                dependency_tree_add_child(tree, tree->root, child);
            child = next;
    tree->rebalance_count++;
    tree->last_rebalance_time = time(NULL);
    return CHRONOS_OK;

chronos_error_t dependency_tree_rebalance_all(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_tree_rebalance(tree, node->stream_id);
            node = node->hash_next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_get_stats(dependency_tree_t* tree, dependency_stats_t* stats) {
    if (tree == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(stats, 0, sizeof(dependency_stats_t));
    stats->total_nodes = tree->active_nodes;
    stats->active_nodes = tree->active_nodes;
    stats->circular_dependencies = tree->circular_dependency_count;
    stats->total_dependency_changes = tree->total_dependency_changes;
    stats->total_weight_changes = tree->total_weight_changes;
    stats->tree_violations = tree->tree_violations;
    stats->tree_errors = tree->tree_errors;
    stats->rebalance_count = tree->rebalance_count;
    stats->peak_nodes = tree->peak_nodes;
    return CHRONOS_OK;

chronos_error_t dependency_tree_cleanup(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_tree_process_closed_nodes(tree);
    return CHRONOS_OK;

chronos_error_t dependency_tree_compact(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_node_t* next = node->hash_next;
            if (node->is_closed) {
                dependency_tree_remove_node(tree, node->stream_id);
            node = next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_detect_corruption(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    tree->corruption_detection_counter++;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            if (node->is_active && node->is_closed) {
                tree->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            if (node->is_blocked && node->unblocked_count > node->blocked_count) {
                tree->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            node = node->hash_next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_recover_corruption(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!tree->is_corrupted) {
        return CHRONOS_OK;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            node->ref_count = 1;
            if (node->unblocked_count > node->blocked_count) {
                node->is_blocked = false;
            if (node->is_closed) {
                node->is_active = false;
            node = node->hash_next;
    tree->is_corrupted = false;
    return CHRONOS_OK;

chronos_error_t dependency_tree_set_max_depth(dependency_tree_t* tree, uint32_t max_depth) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    tree->max_depth = max_depth;
    return CHRONOS_OK;

chronos_error_t dependency_tree_set_max_subtree_size(dependency_tree_t* tree, uint32_t max_size) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    tree->max_subtree_size = max_size;
    return CHRONOS_OK;

chronos_error_t dependency_tree_enable_exclusive(dependency_tree_t* tree, bool enable) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    tree->enable_exclusive_dependencies = enable;
    return CHRONOS_OK;

chronos_error_t dependency_tree_enable_weight_scheduling(dependency_tree_t* tree, bool enable) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    tree->enable_weight_based_scheduling = enable;
    return CHRONOS_OK;

chronos_error_t dependency_tree_add_to_hash(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = node->stream_id % 1000;
    node->hash_next = tree->node_hash[hash_index];
    tree->node_hash[hash_index] = node;
    return CHRONOS_OK;

chronos_error_t dependency_tree_remove_from_hash(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t hash_index = node->stream_id % 1000;
    dependency_node_t* current = tree->node_hash[hash_index];
    dependency_node_t* prev = NULL;
    while (current != NULL) {
        if (current == node) {
            if (prev != NULL) {
                prev->hash_next = current->hash_next;
            } else {
                tree->node_hash[hash_index] = current->hash_next;
            break;
        prev = current;
        current = current->hash_next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_add_child(dependency_tree_t* tree, dependency_node_t* parent, dependency_node_t* child) {
    if (tree == NULL || parent == NULL || child == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    child->next_sibling = parent->first_child;
    if (parent->first_child != NULL) {
        parent->first_child->prev_sibling = child;
    parent->first_child = child;
    child->prev_sibling = NULL;
    if (parent->last_child == NULL) {
        parent->last_child = child;
    parent->dependent_count++;
    child->dependency_count++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_remove_child(dependency_tree_t* tree, dependency_node_t* parent, dependency_node_t* child) {
    if (tree == NULL || parent == NULL || child == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (child->prev_sibling != NULL) {
        child->prev_sibling->next_sibling = child->next_sibling;
    } else {
        parent->first_child = child->next_sibling;
    if (child->next_sibling != NULL) {
        child->next_sibling->prev_sibling = child->prev_sibling;
    } else {
        parent->last_child = child->prev_sibling;
    child->prev_sibling = NULL;
    child->next_sibling = NULL;
    parent->dependent_count--;
    child->dependency_count--;
    return CHRONOS_OK;

chronos_error_t dependency_tree_update_subtree_size(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    dependency_node_t* child = node->first_child;
    while (child != NULL) {
        size += child->subtree_size;
        child = child->next_sibling;
    node->subtree_size = size;
    return CHRONOS_OK;

chronos_error_t dependency_tree_update_depth(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (node->parent != NULL) {
        node->depth = node->parent->depth + 1;
    } else {
        node->depth = 0;
    dependency_node_t* child = node->first_child;
    while (child != NULL) {
        dependency_tree_update_depth(tree, child);
        child = child->next_sibling;
    return CHRONOS_OK;

chronos_error_t dependency_tree_increment_ref_count(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->ref_count++;
    return CHRONOS_OK;

chronos_error_t dependency_tree_decrement_ref_count(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    node->ref_count--;
    return CHRONOS_OK;

chronos_error_t dependency_tree_calculate_subtree_weight(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    uint32_t weight_sum = node->weight;
    dependency_node_t* child = node->first_child;
    while (child != NULL) {
        weight_sum += child->weight;
        child = child->next_sibling;
    node->subtree_weight_sum = weight_sum;
    return CHRONOS_OK;

chronos_error_t dependency_tree_validate_node(dependency_tree_t* tree, dependency_node_t* node) {
    if (tree == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (node->depth > tree->max_depth) {
        tree->tree_violations++;
        node->validation_failures++;
        return CHRONOS_ERROR_INVALID_STATE;
    if (node->subtree_size > tree->max_subtree_size) {
        tree->tree_violations++;
        node->validation_failures++;
        return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t dependency_tree_process_closed_nodes(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_node_t* next = node->hash_next;
            if (node->is_closed && node->ref_count == 0) {
                dependency_tree_remove_node(tree, node->stream_id);
            node = next;
    return CHRONOS_OK;

chronos_error_t dependency_tree_reset(dependency_tree_t* tree) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    for (uint32_t i = 0; i < 1000; i++) {
        dependency_node_t* node = tree->node_hash[i];
        while (node != NULL) {
            dependency_node_t* next = node->hash_next;
            if (node->stream_id != 0) {
                dependency_tree_remove_node(tree, node->stream_id);
            node = next;
    tree->total_nodes_created = 1;
    tree->total_nodes_destroyed = 0;
    tree->total_dependency_changes = 0;
    tree->total_weight_changes = 0;
    tree->circular_dependency_count = 0;
    tree->tree_violations = 0;
    tree->tree_errors = 0;
    tree->rebalance_count = 0;
    return CHRONOS_OK;

chronos_error_t dependency_tree_snapshot(dependency_tree_t* tree, uint8_t* snapshot, size_t* snapshot_len) {
    if (tree == NULL || snapshot == NULL || snapshot_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t required_len = sizeof(uint32_t) * 10 + sizeof(uint8_t) * 5;
    if (*snapshot_len < required_len) {
        *snapshot_len = required_len;
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    size_t offset = 0;
    memcpy(snapshot + offset, &tree->max_nodes, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &tree->active_nodes, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &tree->default_weight, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &tree->total_dependency_changes, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &tree->circular_dependency_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(snapshot + offset, &tree->enable_exclusive_dependencies, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(snapshot + offset, &tree->enable_weight_based_scheduling, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    *snapshot_len = offset;
    return CHRONOS_OK;

chronos_error_t dependency_tree_restore(dependency_tree_t* tree, const uint8_t* snapshot, size_t snapshot_len) {
    if (tree == NULL || snapshot == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t offset = 0;
    memcpy(&tree->max_nodes, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&tree->active_nodes, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&tree->default_weight, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&tree->total_dependency_changes, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&tree->circular_dependency_count, snapshot + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&tree->enable_exclusive_dependencies, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&tree->enable_weight_based_scheduling, snapshot + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    return CHRONOS_OK;

chronos_error_t dependency_tree_update_priority_stats(dependency_tree_t* tree, uint8_t priority) {
    if (tree == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (priority < 256) {
        tree->priority_levels[priority]++;
    return CHRONOS_OK;
