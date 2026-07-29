#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/stream_dependency.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_nodes = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t default_weight = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t tree_id = data[offset++];
    
    if (max_nodes == 0) max_nodes = 100;
    if (max_nodes > 10000) max_nodes = 10000;
    if (default_weight == 0) default_weight = 16;
    
    dependency_tree_t *tree = dependency_tree_create(max_nodes, default_weight, tree_id);
    if (!tree) return;
    
    int32_t num_operations = 0;
    if (offset + 4 <= size) {
        num_operations = *((int32_t*)(data + offset));
        offset += 4;
    }
    
    if (num_operations < 0) num_operations = 10;
    if (num_operations > 1000) num_operations = 100;
    
    for (int i = 0; i < num_operations && offset + 4 <= size; i++) {
        uint8_t op = data[offset++];
        
        switch (op) {
            case 0: {
                if (offset + 12 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t parent_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t weight = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint8_t exclusive = data[offset++];
                    dependency_tree_add_node(tree, stream_id, parent_id, weight, exclusive);
                }
                break;
            }
            case 1: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_remove_node(tree, stream_id);
                }
                break;
            }
            case 2: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t new_parent_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint8_t exclusive = data[offset++];
                    dependency_change_result_t result;
                    dependency_tree_change_dependency(tree, stream_id, new_parent_id, exclusive, &result);
                }
                break;
            }
            case 3: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t weight = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_change_weight(tree, stream_id, weight);
                }
                break;
            }
            case 4: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t priority = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_change_priority(tree, stream_id, priority);
                }
                break;
            }
            case 5: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_block_node(tree, stream_id);
                }
                break;
            }
            case 6: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_unblock_node(tree, stream_id);
                }
                break;
            }
            case 7: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_close_node(tree, stream_id);
                }
                break;
            }
            case 8: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_detect_cycles(tree, stream_id);
                }
                break;
            }
            case 9: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_break_cycles(tree, stream_id);
                }
                break;
            }
            case 10: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_rebalance(tree, stream_id);
                }
                break;
            }
            case 11: {
                dependency_tree_rebalance_all(tree);
                break;
            }
            case 12: {
                dependency_tree_detect_corruption(tree);
                break;
            }
            case 13: {
                dependency_tree_recover_corruption(tree);
                break;
            }
            case 14: {
                if (offset + 4 <= size) {
                    uint32_t max_depth = *((uint32_t*)(data + offset));
                    offset += 4;
                    dependency_tree_set_max_depth(tree, max_depth);
                }
                break;
            }
            case 15: {
                dependency_tree_compact(tree);
                break;
            }
            case 16: {
                dependency_tree_reset(tree);
                break;
            }
            default:
                break;
        }
    }
    
    dependency_tree_destroy(tree);
}
