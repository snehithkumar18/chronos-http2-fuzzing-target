#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/tls/session_cache.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_sessions = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t session_timeout = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t cache_id = data[offset++];
    
    if (max_sessions == 0) max_sessions = 100;
    if (max_sessions > 10000) max_sessions = 10000;
    
    session_cache_t *cache = session_cache_create(max_sessions, session_timeout, cache_id);
    if (!cache) return;
    
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
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t session_data_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (session_data_len > 0 && session_data_len < 4096 && offset + session_data_len <= size) {
                        session_cache_add(cache, session_id, data + offset, session_data_len);
                        offset += session_data_len;
                    }
                }
                break;
            }
            case 1: {
                if (offset + 4 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint8_t output[4096];
                    size_t output_len = 4096;
                    session_cache_lookup(cache, session_id, output, &output_len);
                }
                break;
            }
            case 2: {
                if (offset + 4 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_remove(cache, session_id);
                }
                break;
            }
            case 3: {
                session_cache_cleanup(cache);
                break;
            }
            case 4: {
                session_cache_detect_corruption(cache);
                break;
            }
            case 5: {
                session_cache_recover_corruption(cache);
                break;
            }
            case 6: {
                if (offset + 4 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_increment_ref(cache, session_id);
                }
                break;
            }
            case 7: {
                if (offset + 4 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_decrement_ref(cache, session_id);
                }
                break;
            }
            case 8: {
                session_cache_reset(cache);
                break;
            }
            case 9: {
                if (offset + 8 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t new_timeout = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_update_session(cache, session_id, new_timeout);
                }
                break;
            }
            case 10: {
                if (offset + 8 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_add_dependency(cache, session_id, dependency_id);
                }
                break;
            }
            case 11: {
                session_cache_compact(cache);
                break;
            }
            case 12: {
                if (offset + 4 <= size) {
                    uint32_t session_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    session_cache_invalidate(cache, session_id);
                }
                break;
            }
            default:
                break;
        }
    }
    
    session_cache_destroy(cache);
}
