#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/hpack/dynamic_table_ref.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_size = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_entries = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t table_id = data[offset++];
    
    if (max_size == 0) max_size = 4096;
    if (max_size > 1000000) max_size = 4096;
    if (max_entries == 0) max_entries = 100;
    if (max_entries > 10000) max_entries = 10000;
    
    dynamic_table_ref_t *table = dynamic_table_ref_create(max_size, max_entries, table_id);
    if (!table) return;
    
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
                    uint32_t name_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t value_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (name_len > 0 && name_len < 1024 && value_len > 0 && value_len < 4096 && offset + name_len + value_len <= size) {
                        dynamic_table_ref_add(table, (char*)(data + offset), name_len, 
                                               (char*)(data + offset + name_len), value_len);
                        offset += name_len + value_len;
                    }
                }
                break;
            }
            case 1: {
                if (offset + 4 <= size) {
                    uint32_t index = *((uint32_t*)(data + offset));
                    offset += 4;
                    table_ref_entry_t *entry;
                    dynamic_table_ref_get(table, index, &entry);
                }
                break;
            }
            case 2: {
                if (offset + 8 <= size) {
                    uint32_t name_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t value_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (name_len > 0 && name_len < 1024 && value_len > 0 && value_len < 4096 && offset + name_len + value_len <= size) {
                        table_ref_entry_t *entry;
                        dynamic_table_ref_lookup(table, (char*)(data + offset), name_len,
                                                   (char*)(data + offset + name_len), value_len, &entry);
                        offset += name_len + value_len;
                    }
                }
                break;
            }
            case 3: {
                if (offset + 4 <= size) {
                    uint32_t entry_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_remove(table, entry_id);
                }
                break;
            }
            case 4: {
                if (offset + 4 <= size) {
                    uint32_t new_size = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_resize(table, new_size);
                }
                break;
            }
            case 5: {
                if (offset + 4 <= size) {
                    uint32_t needed_space = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_evict(table, needed_space);
                }
                break;
            }
            case 6: {
                if (offset + 4 <= size) {
                    uint32_t entry_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_increment_ref(table, entry_id);
                }
                break;
            }
            case 7: {
                if (offset + 4 <= size) {
                    uint32_t entry_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_decrement_ref(table, entry_id);
                }
                break;
            }
            case 8: {
                if (offset + 8 <= size) {
                    uint32_t entry_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_add_dependency(table, entry_id, dependency_id);
                }
                break;
            }
            case 9: {
                if (offset + 8 <= size) {
                    uint32_t entry_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    dynamic_table_ref_remove_dependency(table, entry_id, dependency_id);
                }
                break;
            }
            case 10: {
                dynamic_table_ref_detect_corruption(table);
                break;
            }
            case 11: {
                dynamic_table_ref_recover_corruption(table);
                break;
            }
            case 12: {
                dynamic_table_ref_detect_deadlock(table);
                break;
            }
            case 13: {
                dynamic_table_ref_resolve_deadlock(table);
                break;
            }
            case 14: {
                dynamic_table_ref_compact(table);
                break;
            }
            case 15: {
                dynamic_table_ref_reset(table);
                break;
            }
            default:
                break;
        }
    }
    
    dynamic_table_ref_destroy(table);
}
