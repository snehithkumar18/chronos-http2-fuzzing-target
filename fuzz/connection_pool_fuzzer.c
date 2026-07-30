#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/net/connection_pool.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_connections = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_idle_time_ms = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t pool_id = data[offset++];
    
    if (max_connections == 0) max_connections = 100;
    if (max_connections > 10000) max_connections = 10000;
    
    connection_pool_t *pool = connection_pool_create(max_connections, max_idle_time_ms, pool_id);
    if (!pool) return;
    
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
                    uint32_t host_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint16_t port = *((uint16_t*)(data + offset));
                    offset += 2;
                    uint8_t priority = data[offset++];
                    
                    if (host_len > 0 && host_len < 256 && offset + host_len <= size) {
                        acquire_result_t result;
                        connection_pool_acquire(pool, (char*)(data + offset), host_len, port, priority, &result);
                        offset += host_len;
                    }
                }
                break;
            }
            case 1: {
                if (offset + 4 <= size) {
                    uint32_t connection_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    pooled_connection_t *conn;
                    connection_pool_get_connection_by_id(pool, connection_id, &conn);
                    if (conn) {
                        connection_pool_release(pool, conn);
                    }
                }
                break;
            }
            case 2: {
                if (offset + 4 <= size) {
                    uint32_t connection_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    pooled_connection_t *conn;
                    connection_pool_get_connection_by_id(pool, connection_id, &conn);
                    if (conn) {
                        connection_pool_close(pool, conn);
                    }
                }
                break;
            }
            case 3: {
                connection_pool_cleanup(pool);
                break;
            }
            case 4: {
                connection_pool_health_check(pool);
                break;
            }
            case 5: {
                if (offset + 4 <= size) {
                    uint32_t count = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_pool_scale_up(pool, count);
                }
                break;
            }
            case 6: {
                if (offset + 4 <= size) {
                    uint32_t count = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_pool_scale_down(pool, count);
                }
                break;
            }
            case 7: {
                connection_pool_detect_corruption(pool);
                break;
            }
            case 8: {
                connection_pool_recover_corruption(pool);
                break;
            }
            case 9: {
                if (offset + 4 <= size) {
                    uint32_t max_conn = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_pool_set_max_connections(pool, max_conn);
                }
                break;
            }
            case 10: {
                if (offset + 4 <= size) {
                    uint32_t timeout_ms = *((uint32_t*)(data + offset));
                    offset += 4;
                    connection_pool_set_timeout(pool, timeout_ms);
                }
                break;
            }
            case 11: {
                connection_pool_compact(pool);
                break;
            }
            case 12: {
                connection_pool_reset(pool);
                break;
            }
            case 13: {
                if (offset + 8 <= size) {
                    uint32_t connection_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    int32_t delta = *((int32_t*)(data + offset));
                    offset += 4;
                    pooled_connection_t *conn;
                    connection_pool_get_connection_by_id(pool, connection_id, &conn);
                    if (conn) {
                        if (delta > 0) {
                            connection_pool_increment_ref_count(pool, conn);
                        } else {
                            connection_pool_decrement_ref_count(pool, conn);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    connection_pool_destroy(pool);
}
