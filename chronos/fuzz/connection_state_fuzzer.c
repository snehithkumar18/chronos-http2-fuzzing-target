#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/connection_state.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_events = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t connection_id = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t state_id = data[offset++];
    
    if (max_events == 0) max_events = 100;
    if (max_events > 10000) max_events = 10000;
    
    connection_state_t *state = connection_state_create(max_events, connection_id, state_id);
    if (!state) return;
    
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
                if (offset + 8 <= size) {
                    uint32_t event_type = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t event_data_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (event_data_len > 0 && event_data_len < 4096 && offset + event_data_len <= size) {
                        connection_state_add_event(state, event_type, data + offset, event_data_len);
                        offset += event_data_len;
                    }
                }
                break;
            }
            case 1: {
                connection_state_process_events(state);
                break;
            }
            case 2: {
                if (offset + 4 <= size) {
                    uint32_t new_state = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_state_transition(state, new_state);
                }
                break;
            }
            case 3: {
                if (offset + 4 <= size) {
                    uint32_t timeout_ms = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_state_set_timeout(state, timeout_ms);
                }
                break;
            }
            case 4: {
                connection_state_block(state);
                break;
            }
            case 5: {
                connection_state_unblock(state);
                break;
            }
            case 6: {
                connection_state_detect_corruption(state);
                break;
            }
            case 7: {
                connection_state_recover_corruption(state);
                break;
            }
            case 8: {
                connection_state_reset(state);
                break;
            }
            case 9: {
                if (offset + 4 <= size) {
                    uint32_t event_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_state_remove_event(state, event_id);
                }
                break;
            }
            case 10: {
                connection_state_compact(state);
                break;
            }
            case 11: {
                connection_state_validate(state);
                break;
            }
            case 12: {
                connection_state_abort(state);
                break;
            }
            case 13: {
                connection_state_commit(state);
                break;
            }
            default:
                break;
        }
    }
    
    connection_state_destroy(state);
}
