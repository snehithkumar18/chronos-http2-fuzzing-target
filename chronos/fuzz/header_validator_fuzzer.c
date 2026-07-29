#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/http/header_validator.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_header_count = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_header_size = *((uint32_t*)(data + offset));
    offset += 4;
    uint8_t context_id = data[offset++];
    
    if (max_header_count == 0) max_header_count = 100;
    if (max_header_count > 10000) max_header_count = 10000;
    if (max_header_size == 0) max_header_size = 4096;
    if (max_header_size > 100000) max_header_size = 4096;
    
    validation_context_t *ctx = validation_context_create(max_header_count, max_header_size, context_id);
    if (!ctx) return;
    
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
                        validation_context_add_header(ctx, (char*)(data + offset), name_len,
                                                    (char*)(data + offset + name_len), value_len);
                        offset += name_len + value_len;
                    }
                }
                break;
            }
            case 1: {
                if (offset + 8 <= size) {
                    uint32_t field_name_len = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint8_t rule_type = data[offset++];
                    uint32_t min_length = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t max_length = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (field_name_len > 0 && field_name_len < 256 && offset + field_name_len <= size) {
                        validation_context_add_rule(ctx, (char*)(data + offset), field_name_len,
                                                   rule_type, min_length, max_length);
                        offset += field_name_len;
                    }
                }
                break;
            }
            case 2: {
                validation_result_t result;
                validation_context_validate(ctx, &result);
                break;
            }
            case 3: {
                if (offset + 4 <= size) {
                    uint32_t header_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    validation_result_t result;
                    validation_context_validate_header(ctx, header_id, &result);
                }
                break;
            }
            case 4: {
                validation_result_t result;
                validation_context_detect_bypass(ctx, &result);
                break;
            }
            case 5: {
                validation_result_t result;
                validation_context_detect_injection(ctx, &result);
                break;
            }
            case 6: {
                validation_result_t result;
                validation_context_detect_duplicates(ctx, &result);
                break;
            }
            case 7: {
                validation_result_t result;
                validation_context_validate_order(ctx, &result);
                break;
            }
            case 8: {
                validation_result_t result;
                validation_context_validate_size(ctx, &result);
                break;
            }
            case 9: {
                validation_result_t result;
                validation_context_validate_chars(ctx, &result);
                break;
            }
            case 10: {
                validation_result_t result;
                validation_context_validate_dependencies(ctx, &result);
                break;
            }
            case 11: {
                if (offset + 8 <= size) {
                    uint32_t header_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t dependency_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    validation_context_add_header_dependency(ctx, header_id, dependency_id);
                }
                break;
            }
            case 12: {
                validation_context_detect_corruption(ctx);
                break;
            }
            case 13: {
                validation_context_recover_corruption(ctx);
                break;
            }
            case 14: {
                validation_context_reset(ctx);
                break;
            }
            case 15: {
                validation_context_compact(ctx);
                break;
            }
            default:
                break;
        }
    }
    
    validation_context_destroy(ctx);
}
