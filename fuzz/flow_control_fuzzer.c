#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/flow_control.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_streams = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t initial_window_size = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t manager_id = data[offset++];
    
    if (max_streams == 0) max_streams = 100;
    if (max_streams > 10000) max_streams = 10000;
    if (initial_window_size == 0) initial_window_size = 65535;
    
    flow_control_manager_t *manager = flow_control_manager_create(max_streams, initial_window_size, manager_id);
    if (!manager) return;
    
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
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_window_t *window;
                    flow_control_create_stream_window(manager, stream_id, &window);
                }
                break;
            }
            case 1: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_destroy_stream_window(manager, stream_id);
                }
                break;
            }
            case 2: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    int32_t delta = *((int32_t*)(data + offset));
                    offset += 4;
                    window_update_result_t result;
                    flow_control_update_window(manager, stream_id, delta, &result);
                }
                break;
            }
            case 3: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t amount = *((uint32_t*)(data + offset));
                    offset += 4;
                    window_update_result_t result;
                    flow_control_consume_window(manager, stream_id, amount, &result);
                }
                break;
            }
            case 4: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t window_size = *((uint32_t*)(data + offset));
                    offset += 4;
                    window_update_result_t result;
                    flow_control_advertise_window(manager, stream_id, window_size, &result);
                }
                break;
            }
            case 5: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_block_window(manager, stream_id);
                }
                break;
            }
            case 6: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_unblock_window(manager, stream_id);
                }
                break;
            }
            case 7: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_auto_update_window(manager, stream_id);
                }
                break;
            }
            case 8: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    double scaling_factor = *((double*)(data + offset));
                    offset += 8;
                    flow_control_scale_window(manager, stream_id, scaling_factor);
                }
                break;
            }
            case 9: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_add_window_dependency(manager, stream_id, dependency_id);
                }
                break;
            }
            case 10: {
                if (offset + 8 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_remove_window_dependency(manager, stream_id, dependency_id);
                }
                break;
            }
            case 11: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_detect_deadlock(manager, stream_id);
                }
                break;
            }
            case 12: {
                if (offset + 4 <= size) {
                    uint32_t stream_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    flow_control_resolve_deadlock(manager, stream_id);
                }
                break;
            }
            case 13: {
                flow_control_cleanup(manager);
                break;
            }
            case 14: {
                flow_control_detect_corruption(manager);
                break;
            }
            case 15: {
                flow_control_recover_corruption(manager);
                break;
            }
            case 16: {
                flow_control_reset(manager);
                break;
            }
            default:
                break;
        }
    }
    
    flow_control_manager_destroy(manager);
}
