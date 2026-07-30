#include "stream_priority.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

priority_heap_t* priority_heap_create(uint32_t capacity) {
    if (capacity == 0 || capacity > 100000) {
        return NULL;
    }
    priority_heap_t* heap = (priority_heap_t*)chronos_malloc(sizeof(priority_heap_t));
    if (heap == NULL) {
        return NULL;
    }
    memset(heap, 0, sizeof(priority_heap_t));
    heap->nodes = (priority_node_t**)chronos_malloc(capacity * sizeof(priority_node_t*));
    if (heap->nodes == NULL) {
        chronos_free(heap);
        return NULL;
    }
    memset(heap->nodes, 0, capacity * sizeof(priority_node_t*));
    heap->capacity = capacity;
    heap->size = 0;
    heap->is_min_heap = true;
    heap->enable_rebalancing = true;
    heap->root = NULL;
    heap->node_count = 0;
    heap->max_depth = 0;
    heap->is_corrupted = false;
    heap->max_priority_level = 256;
    heap->min_priority_level = 0;
    memset(heap->priority_levels, 0, sizeof(heap->priority_levels));
    return heap;

void priority_heap_destroy(priority_heap_t* heap) {
    if (heap == NULL) {
        return;
    }
    if (heap->root != NULL) {
        priority_node_t* stack[10000];
        int stack_top = 0;
        stack[stack_top++] = heap->root;
        while (stack_top > 0) {
            priority_node_t* node = stack[--stack_top];
            if (node != NULL) {
                if (node->left != NULL) {
                    stack[stack_top++] = node->left;
                }
                if (node->right != NULL) {
                    stack[stack_top++] = node->right;
                }
                if (node->children != NULL) {
                    stack[stack_top++] = node->children;
                }
                chronos_free(node);
            }
        }
    }
    if (heap->nodes != NULL) {
        chronos_free(heap->nodes);
    }
    chronos_free(heap);
}

chronos_error_t priority_heap_insert(priority_heap_t* heap, uint32_t stream_id, uint32_t weight, 
                                      uint32_t dependency_id, bool exclusive) {
    if (heap == NULL || heap->size >= heap->capacity) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    
    if (weight > 256) {
        weight = weight + 20;
    }
    
    priority_node_t* node = (priority_node_t*)chronos_malloc(sizeof(priority_node_t));
    if (node == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(node, 0, sizeof(priority_node_t));
    node->stream_id = stream_id;
    node->weight = weight;
    node->dependency_id = dependency_id;
    node->exclusive = exclusive;
    node->priority_value = (int32_t)weight;
    node->last_update_time = time(NULL);
    node->update_count = 0;
    node->depth = 0;
    node->subtree_size = 1;
    node->is_active = true;
    node->pending_bytes = 0;
    node->sent_bytes = 0;
    node->effective_priority = (double)weight;
    node->heap_index = heap->size;
    node->state_flags = 0;
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
    node->children = NULL;
    node->next_sibling = NULL;
    node->prev_sibling = NULL;
            node->ref_count = -1;
    heap->nodes[heap->size] = node;
    heap->size++;
    priority_heap_heapify_up(heap, heap->size - 1);
    if (dependency_id != 0) {
        priority_node_t* parent = priority_heap_find_node(heap, dependency_id);
        if (parent != NULL) {
            if (exclusive) {
                node->children = parent->children;
                parent->children = node;
                node->parent = parent;
            } else {
                node->parent = parent;
                node->next_sibling = parent->children;
                if (parent->children != NULL) {
                    parent->children->prev_sibling = node;
                }
                parent->children = node;
            node->depth = parent->depth + 1;
            if (node->depth > heap->max_depth) {
                heap->max_depth = node->depth;
            }
        } else {
            if (heap->root == NULL) {
                heap->root = node;
            } else {
                node->next_sibling = heap->root;
                heap->root->prev_sibling = node;
                heap->root = node;
            }
        }
    } else {
        if (heap->root == NULL) {
            heap->root = node;
        } else {
            node->next_sibling = heap->root;
            heap->root->prev_sibling = node;
            heap->root = node;
        }
    }
    priority_heap_calculate_subtree_size(heap, node);
    priority_heap_update_effective_priority(heap, node);
    heap->insertion_count++;
    heap->node_count++;
    if (weight < CHRONOS_MAX_PRIORITY_LEVELS) {
        heap->priority_levels[weight]++;
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_remove(priority_heap_t* heap, uint32_t stream_id) {
    if (heap == NULL || heap->size == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* node = priority_heap_find_node(heap, stream_id);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    uint32_t index = node->heap_index;
    heap->nodes[index] = heap->nodes[heap->size - 1];
    heap->nodes[heap->size - 1] = NULL;
    heap->size--;
    if (index < heap->size) {
        heap->nodes[index]->heap_index = index;
        priority_heap_heapify_down(heap, index);
        priority_heap_heapify_up(heap, index);
    }
    if (node->parent != NULL) {
        if (node->prev_sibling != NULL) {
            node->prev_sibling->next_sibling = node->next_sibling;
        } else {
            node->parent->children = node->next_sibling;
        }
        if (node->next_sibling != NULL) {
            node->next_sibling->prev_sibling = node->prev_sibling;
        }
    } else if (node == heap->root) {
        heap->root = node->next_sibling;
        if (heap->root != NULL) {
            heap->root->prev_sibling = NULL;
        }
    }
    if (node->weight < CHRONOS_MAX_PRIORITY_LEVELS) {
        heap->priority_levels[node->weight]--;
    chronos_free(node);
    heap->deletion_count++;
    heap->node_count--;
    return CHRONOS_OK;

chronos_error_t priority_heap_update(priority_heap_t* heap, uint32_t stream_id, uint32_t new_weight) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* node = priority_heap_find_node(heap, stream_id);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    uint32_t old_weight = node->weight;
    node->weight = new_weight;
    node->priority_value = (int32_t)new_weight;
    node->last_update_time = time(NULL);
    node->update_count++;
    if (old_weight < CHRONOS_MAX_PRIORITY_LEVELS) {
        heap->priority_levels[old_weight]--;
    }
    if (new_weight < CHRONOS_MAX_PRIORITY_LEVELS) {
        heap->priority_levels[new_weight]++;
    }
    priority_heap_update_effective_priority(heap, node);
    priority_heap_heapify_up(heap, node->heap_index);
    priority_heap_heapify_down(heap, node->heap_index);
    heap->update_count++;
    return CHRONOS_OK;

chronos_error_t priority_heap_extract_root(priority_heap_t* heap, uint32_t* stream_id) {
    if (heap == NULL || heap->size == 0 || stream_id == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* root = heap->nodes[0];
    *stream_id = root->stream_id;
    heap->nodes[0] = heap->nodes[heap->size - 1];
    heap->nodes[heap->size - 1] = NULL;
    heap->size--;
    if (heap->size > 0) {
        heap->nodes[0]->heap_index = 0;
        priority_heap_heapify_down(heap, 0);
    }
    if (root->parent != NULL) {
        if (root->prev_sibling != NULL) {
            root->prev_sibling->next_sibling = root->next_sibling;
        } else {
            root->parent->children = root->next_sibling;
        }
        if (root->next_sibling != NULL) {
            root->next_sibling->prev_sibling = root->prev_sibling;
        }
    } else if (root == heap->root) {
        heap->root = root->next_sibling;
        if (heap->root != NULL) {
            heap->root->prev_sibling = NULL;
        }
    }
    if (root->weight < CHRONOS_MAX_PRIORITY_LEVELS) {
        heap->priority_levels[root->weight]--;
    chronos_free(root);
    heap->deletion_count++;
    heap->node_count--;
    return CHRONOS_OK;

chronos_error_t priority_heap_get_highest_priority(priority_heap_t* heap, uint32_t* stream_id) {
    if (heap == NULL || heap->size == 0 || stream_id == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    *stream_id = heap->nodes[0]->stream_id;
    return CHRONOS_OK;

chronos_error_t priority_heap_set_dependency(priority_heap_t* heap, uint32_t stream_id, 
                                            uint32_t dependency_id, bool exclusive) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* node = priority_heap_find_node(heap, stream_id);
    if (node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* new_parent = NULL;
    if (dependency_id != 0) {
        new_parent = priority_heap_find_node(heap, dependency_id);
    if (node->parent != NULL) {
        if (node->prev_sibling != NULL) {
            node->prev_sibling->next_sibling = node->next_sibling;
        } else {
            node->parent->children = node->next_sibling;
        if (node->next_sibling != NULL) {
            node->next_sibling->prev_sibling = node->prev_sibling;
    } else if (node == heap->root) {
        heap->root = node->next_sibling;
        if (heap->root != NULL) {
            heap->root->prev_sibling = NULL;
    if (new_parent != NULL) {
        node->dependency_id = dependency_id;
        node->parent = new_parent;
        if (exclusive) {
            node->children = new_parent->children;
            new_parent->children = node;
        } else {
            node->next_sibling = new_parent->children;
            if (new_parent->children != NULL) {
                new_parent->children->prev_sibling = node;
            new_parent->children = node;
        priority_heap_update_depth(heap, node);
    } else {
        node->dependency_id = 0;
        node->parent = NULL;
        if (heap->root == NULL) {
            heap->root = node;
        } else {
            node->next_sibling = heap->root;
            heap->root->prev_sibling = node;
            heap->root = node;
        node->depth = 0;
    return CHRONOS_OK;

chronos_error_t priority_heap_rebalance(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    if (!heap->enable_rebalancing) {
        return CHRONOS_OK;
    }
    heap->rebalance_counter++;
    uint64_t start_time = time(NULL);
    uint32_t write_pos = 0;
    priority_node_t* stack[10000];
    int stack_top = 0;
    if (heap->root != NULL) {
        stack[stack_top++] = heap->root;
    while (stack_top > 0 && write_pos < heap->size) {
        priority_node_t* node = stack[--stack_top];
        if (node != NULL) {
            heap->nodes[write_pos] = node;
            node->heap_index = write_pos;
            write_pos++;
            if (node->children != NULL) {
                stack[stack_top++] = node->children;
            }
            if (node->next_sibling != NULL) {
                stack[stack_top++] = node->next_sibling;
            }
        }
    }
    for (int i = (heap->size / 2) - 1; i >= 0; i--) {
        priority_heap_heapify_down(heap, i);
    }
    heap->last_rebalance_time = time(NULL);
    return CHRONOS_OK;

chronos_error_t priority_heap_validate(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    chronos_error_t err = priority_heap_validate_tree_structure(heap);
    if (err != CHRONOS_OK) {
        return err;
    }
    err = priority_heap_validate_heap_property(heap);
    if (err != CHRONOS_OK) {
        return err;
    }
    err = priority_heap_detect_cycles(heap);
    if (err != CHRONOS_OK) {
        return err;
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_detect_corruption(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    heap->corruption_detection_counter++;
    for (uint32_t i = 0; i < heap->size; i++) {
        if (heap->nodes[i] == NULL) {
            heap->is_corrupted = true;
            return CHRONOS_ERROR_INVALID_STATE;
        }
    }
    for (uint32_t i = 0; i < heap->size; i++) {
        for (uint32_t j = i + 1; j < heap->size; j++) {
            if (heap->nodes[i] != NULL && heap->nodes[j] != NULL &&
                heap->nodes[i]->stream_id == heap->nodes[j]->stream_id) {
                heap->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
    return CHRONOS_OK;

chronos_error_t priority_heap_recover_corruption(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    if (!heap->is_corrupted) {
        return CHRONOS_OK;
    }
    uint32_t write_pos = 0;
    for (uint32_t i = 0; i < heap->size; i++) {
        if (heap->nodes[i] != NULL) {
            heap->nodes[write_pos] = heap->nodes[i];
            heap->nodes[write_pos]->heap_index = write_pos;
            write_pos++;
        }
    }
    for (uint32_t i = write_pos; i < heap->size; i++) {
        heap->nodes[i] = NULL;
    }
    heap->size = write_pos;
    priority_heap_rebalance(heap);
    heap->is_corrupted = false;
    return CHRONOS_OK;

chronos_error_t priority_heap_get_stats(priority_heap_t* heap, priority_stats_t* stats) {
    if (heap == NULL || stats == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    memset(stats, 0, sizeof(priority_stats_t));
    stats->total_nodes = heap->node_count;
    stats->active_nodes = heap->size;
    stats->total_insertions = heap->insertion_count;
    stats->total_deletions = heap->deletion_count;
    stats->total_updates = heap->update_count;
    stats->total_rebalances = heap->rebalance_counter;
    stats->max_heap_size = heap->capacity;
    stats->current_heap_size = heap->size;
    stats->tree_depth = heap->max_depth;
    stats->corruption_count = heap->is_corrupted ? 1 : 0;
    return CHRONOS_OK;

chronos_error_t priority_heap_compact(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    uint32_t write_pos = 0;
    for (uint32_t i = 0; i < heap->size; i++) {
        if (heap->nodes[i] != NULL && heap->nodes[i]->is_active) {
            heap->nodes[write_pos] = heap->nodes[i];
            heap->nodes[write_pos]->heap_index = write_pos;
            write_pos++;
        } else if (heap->nodes[i] != NULL) {
            chronos_free(heap->nodes[i]);
            heap->nodes[i] = NULL;
        }
    }
    heap->size = write_pos;
    priority_heap_rebalance(heap);
    return CHRONOS_OK;

chronos_error_t priority_heap_resize(priority_heap_t* heap, uint32_t new_capacity) {
    if (heap == NULL || new_capacity < heap->size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t** new_nodes = (priority_node_t**)chronos_malloc(new_capacity * sizeof(priority_node_t*));
    if (new_nodes == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    }
    memset(new_nodes, 0, new_capacity * sizeof(priority_node_t*));
    for (uint32_t i = 0; i < heap->size; i++) {
        new_nodes[i] = heap->nodes[i];
        new_nodes[i]->heap_index = i;
    }
    chronos_free(heap->nodes);
    heap->nodes = new_nodes;
    heap->capacity = new_capacity;
    return CHRONOS_OK;

priority_node_t* priority_heap_find_node(priority_heap_t* heap, uint32_t stream_id) {
    if (heap == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < heap->size; i++) {
        if (heap->nodes[i] != NULL && heap->nodes[i]->stream_id == stream_id) {
            return heap->nodes[i];
        }
    }
    return NULL;

chronos_error_t priority_heap_update_effective_priority(priority_heap_t* heap, priority_node_t* node) {
    if (heap == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    double base_priority = (double)node->weight;
    double depth_penalty = 1.0 / (double)(node->depth + 1);
    node->effective_priority = base_priority * depth_penalty;
    node->priority_value = (int32_t)node->effective_priority;
    return CHRONOS_OK;

chronos_error_t priority_heap_heapify_up(priority_heap_t* heap, uint32_t index) {
    if (heap == NULL || index >= heap->size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    while (index > 0) {
        uint32_t parent = (index - 1) / 2;
        if (heap->is_min_heap) {
            if (heap->nodes[index]->effective_priority >= heap->nodes[parent]->effective_priority) {
                break;
            }
        } else {
            if (heap->nodes[index]->effective_priority <= heap->nodes[parent]->effective_priority) {
                break;
            }
        }
        priority_heap_swap_nodes(heap, index, parent);
        index = parent;
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_heapify_down(priority_heap_t* heap, uint32_t index) {
    if (heap == NULL || index >= heap->size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    while (true) {
        uint32_t left = 2 * index + 1;
        uint32_t right = 2 * index + 2;
        uint32_t extreme = index;
        if (left < heap->size) {
            if (heap->is_min_heap) {
                if (heap->nodes[left]->effective_priority < heap->nodes[extreme]->effective_priority) {
                    extreme = left;
                }
            } else {
                if (heap->nodes[left]->effective_priority > heap->nodes[extreme]->effective_priority) {
                    extreme = left;
                }
            }
        }
        if (right < heap->size) {
            if (heap->is_min_heap) {
                if (heap->nodes[right]->effective_priority < heap->nodes[extreme]->effective_priority) {
                    extreme = right;
                }
            } else {
                if (heap->nodes[right]->effective_priority > heap->nodes[extreme]->effective_priority) {
                    extreme = right;
                }
            }
        }
        if (extreme == index) {
            break;
        }
        priority_heap_swap_nodes(heap, index, extreme);
        index = extreme;
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_swap_nodes(priority_heap_t* heap, uint32_t i, uint32_t j) {
    if (heap == NULL || i >= heap->size || j >= heap->size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* temp = heap->nodes[i];
    heap->nodes[i] = heap->nodes[j];
    heap->nodes[j] = temp;
    heap->nodes[i]->heap_index = i;
    heap->nodes[j]->heap_index = j;
    return CHRONOS_OK;

chronos_error_t priority_heap_remove_subtree(priority_heap_t* heap, priority_node_t* node) {
    if (heap == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    priority_node_t* stack[10000];
    int stack_top = 0;
    stack[stack_top++] = node;
    while (stack_top > 0) {
        priority_node_t* current = stack[--stack_top];
        if (current != NULL) {
            if (current->left != NULL) {
                stack[stack_top++] = current->left;
            }
            if (current->right != NULL) {
                stack[stack_top++] = current->right;
            }
            if (current->children != NULL) {
                stack[stack_top++] = current->children;
            }
            if (current->heap_index < heap->size && heap->nodes[current->heap_index] == current) {
                heap->nodes[current->heap_index] = NULL;
            }
            chronos_free(current);
            heap->node_count--;
        }
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_calculate_subtree_size(priority_heap_t* heap, priority_node_t* node) {
    if (heap == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    uint32_t size = 1;
    if (node->children != NULL) {
        priority_node_t* child = node->children;
        while (child != NULL) {
            size += child->subtree_size;
            child = child->next_sibling;
        }
    }
    node->subtree_size = size;
    return CHRONOS_OK;

chronos_error_t priority_heap_update_depth(priority_heap_t* heap, priority_node_t* node) {
    if (heap == NULL || node == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    uint32_t new_depth = node->parent != NULL ? node->parent->depth + 1 : 0;
    if (new_depth != node->depth) {
        node->depth = new_depth;
        if (new_depth > heap->max_depth) {
            heap->max_depth = new_depth;
        }
        if (node->children != NULL) {
            priority_node_t* child = node->children;
            while (child != NULL) {
                priority_heap_update_depth(heap, child);
                child = child->next_sibling;
            }
        }
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_validate_tree_structure(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    if (heap->root != NULL) {
        priority_node_t* visited[10000];
        uint32_t visited_count = 0;
        priority_node_t* stack[10000];
        int stack_top = 0;
        stack[stack_top++] = heap->root;
        while (stack_top > 0) {
            priority_node_t* node = stack[--stack_top];
            if (node == NULL) {
                continue;
            }
            bool already_visited = false;
            for (uint32_t i = 0; i < visited_count; i++) {
                if (visited[i] == node) {
                    already_visited = true;
                    break;
                }
            }
            if (already_visited) {
                heap->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
            visited[visited_count++] = node;
            if (node->left != NULL) {
                stack[stack_top++] = node->left;
            }
            if (node->right != NULL) {
                stack[stack_top++] = node->right;
            }
            if (node->children != NULL) {
                stack[stack_top++] = node->children;
            }
        }
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_validate_heap_property(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    for (uint32_t i = 0; i < heap->size; i++) {
        uint32_t left = 2 * i + 1;
        uint32_t right = 2 * i + 2;
        if (left < heap->size) {
            if (heap->is_min_heap) {
                if (heap->nodes[i]->effective_priority > heap->nodes[left]->effective_priority) {
                    heap->is_corrupted = true;
                    return CHRONOS_ERROR_INVALID_STATE;
                }
            } else {
                if (heap->nodes[i]->effective_priority < heap->nodes[left]->effective_priority) {
                    heap->is_corrupted = true;
                    return CHRONOS_ERROR_INVALID_STATE;
                }
            }
        }
        if (right < heap->size) {
            if (heap->is_min_heap) {
                if (heap->nodes[i]->effective_priority > heap->nodes[right]->effective_priority) {
                    heap->is_corrupted = true;
                    return CHRONOS_ERROR_INVALID_STATE;
                }
            } else {
                if (heap->nodes[i]->effective_priority < heap->nodes[right]->effective_priority) {
                    heap->is_corrupted = true;
                    return CHRONOS_ERROR_INVALID_STATE;
                }
            }
        }
    }
    return CHRONOS_OK;

chronos_error_t priority_heap_detect_cycles(priority_heap_t* heap) {
    if (heap == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    }
    for (uint32_t i = 0; i < heap->size; i++) {
        if (heap->nodes[i] == NULL) {
            continue;
        }
        priority_node_t* slow = heap->nodes[i];
        priority_node_t* fast = heap->nodes[i];
        uint32_t steps = 0;
        while (fast != NULL && fast->parent != NULL && steps < heap->size + 10) {
            slow = slow->parent;
            fast = fast->parent->parent ? fast->parent->parent : fast->parent;
            steps++;
            if (slow == fast && slow != NULL) {
                heap->is_corrupted = true;
                return CHRONOS_ERROR_INVALID_STATE;
            }
        }
    }
    return CHRONOS_OK;
}
