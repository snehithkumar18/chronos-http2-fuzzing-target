#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/utils/memory_pool.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return;
    
    size_t offset = 0;
    
    uint32_t block_size = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_blocks = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t pool_id = data[offset++];
    
    if (block_size == 0) block_size = 1024;
    if (max_blocks == 0 || max_blocks > 1000) max_blocks = 100;
    
    memory_pool_t *pool = memory_pool_create(block_size, max_blocks, pool_id);
    if (!pool) return;
    
    int32_t num_operations = 0;
    if (offset + 4 <= size) {
        num_operations = *((int32_t*)(data + offset));
        offset += 4;
    }
    
    if (num_operations < 0) num_operations = 10;
    if (num_operations > 1000) num_operations = 100;
    
    void *allocations[100];
    int alloc_count = 0;
    
    for (int i = 0; i < num_operations && offset + 4 <= size; i++) {
        uint8_t op = data[offset++];
        
        switch (op) {
            case 0: {
                if (offset + 4 <= size) {
                    uint32_t alloc_size = *((uint32_t*)(data + offset));
                    offset += 4;
                    if (alloc_size > block_size) alloc_size = block_size;
                    if (alloc_count < 100) {
                        allocations[alloc_count] = memory_pool_alloc(pool, alloc_size, i);
                        if (allocations[alloc_count]) {
                            alloc_count++;
                        }
                    }
                }
                break;
            }
            case 1: {
                if (alloc_count > 0 && offset + 4 <= size) {
                    uint32_t idx = *((uint32_t*)(data + offset)) % alloc_count;
                    offset += 4;
                    memory_pool_free(pool, allocations[idx]);
                    allocations[idx] = NULL;
                }
                break;
            }
            case 2: {
                memory_pool_defragment(pool);
                break;
            }
            case 3: {
                memory_pool_coalesce(pool);
                break;
            }
            case 4: {
                memory_pool_compact(pool);
                break;
            }
            case 5: {
                memory_pool_detect_corruption(pool);
                break;
            }
            case 6: {
                memory_pool_recover_corruption(pool);
                break;
            }
            case 7: {
                if (alloc_count > 0 && offset + 4 <= size) {
                    uint32_t idx = *((uint32_t*)(data + offset)) % alloc_count;
                    offset += 4;
                    if (offset + 4 <= size) {
                        int32_t delta = *((int32_t*)(data + offset));
                        offset += 4;
                        memory_block_t *block = memory_pool_find_block(pool, allocations[idx]);
                        if (block) {
                            memory_pool_update_ref_count(pool, block, delta);
                        }
                    }
                }
                break;
            }
            case 8: {
                memory_pool_reset(pool);
                alloc_count = 0;
                break;
            }
            default:
                break;
        }
    }
    
    for (int i = 0; i < alloc_count; i++) {
        if (allocations[i]) {
            memory_pool_free(pool, allocations[i]);
        }
    }
    
    memory_pool_destroy(pool);
}
