#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/stream_priority.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t capacity = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t is_min_heap = data[offset++];
    uint8_t heap_id = data[offset++];
    
    if (capacity == 0) capacity = 100;
    if (capacity > 10000) capacity = 10000;
    
    priority_heap_t *heap = priority_heap_create(capacity, is_min_heap);
    if (!heap) return;
    
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
                    uint32_t priority = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t weight = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_insert(heap, stream_id, priority, weight);
                }
                break;
            }
            case 1: {
                priority_node_t *node = priority_heap_extract(heap);
                if (node) {
                    chronos_free(node);
                }
                break;
            }
            case 2: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_remove(heap, stream_id);
                }
                break;
            }
            case 3: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t new_priority = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_update_priority(heap, stream_id, new_priority);
                }
                break;
            }
            case 4: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t new_weight = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_update_weight(heap, stream_id, new_weight);
                }
                break;
            }
            case 5: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_add_dependency(heap, stream_id, dependency_id);
                }
                break;
            }
            case 6: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_remove_dependency(heap, stream_id, dependency_id);
                }
                break;
            }
            case 7: {
                priority_heap_rebalance(heap);
                break;
            }
            case 8: {
                priority_heap_detect_corruption(heap);
                break;
            }
            case 9: {
                priority_heap_recover_corruption(heap);
                break;
            }
            case 10: {
                priority_heap_detect_deadlock(heap);
                break;
            }
            case 11: {
                priority_heap_resolve_deadlock(heap);
                break;
            }
            case 12: {
                priority_heap_compact(heap);
                break;
            }
            case 13: {
                priority_heap_reset(heap);
                break;
            }
            case 14: {
                if (offset + 4 <= size) {
                    uint32_t new_capacity = *((uint32_t*)(data + offset));
                    offset += 4;
                    priority_heap_resize(heap, new_capacity);
                }
                break;
            }
            default:
                break;
        }
    }
    
    priority_heap_destroy(heap);
}
