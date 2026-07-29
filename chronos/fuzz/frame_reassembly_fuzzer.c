#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/frame_reassembly.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12) return;
    
    size_t offset = 0;
    
    uint32_t max_fragments = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_frame_size = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t stream_id = *((uint32_t*)(data + offset));
    offset += 4;
    
    if (max_fragments == 0) max_fragments = 100;
    if (max_fragments > 1000) max_fragments = 1000;
    if (max_frame_size == 0) max_frame_size = 65535;
    if (max_frame_size > 1000000) max_frame_size = 65535;
    
    frame_reassembly_t *reassembly = frame_reassembly_create(max_fragments, max_frame_size, stream_id);
    if (!reassembly) return;
    
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
                    uint32_t fragment_offset = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t fragment_length = *((uint32_t*)(data + offset));
                    offset += 4;
                    uint32_t fragment_flags = *((uint32_t*)(data + offset));
                    offset += 4;
                    
                    if (fragment_length > 0 && offset + fragment_length <= size) {
                        frame_reassembly_add_fragment(reassembly, fragment_offset, 
                                                        data + offset, fragment_length, 
                                                        fragment_flags);
                        offset += fragment_length;
                    }
                }
                break;
            }
            case 1: {
                uint8_t output[65536];
                size_t output_len = 65536;
                frame_reassembly_assemble(reassembly, output, &output_len);
                break;
            }
            case 2: {
                frame_reassembly_compact(reassembly);
                break;
            }
            case 3: {
                frame_reassembly_detect_corruption(reassembly);
                break;
            }
            case 4: {
                frame_reassembly_recover_corruption(reassembly);
                break;
            }
            case 5: {
                if (offset + 4 <= size) {
                    uint32_t fragment_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    frame_reassembly_remove_fragment(reassembly, fragment_id);
                }
                break;
            }
            case 6: {
                frame_reassembly_reset(reassembly);
                break;
            }
            case 7: {
                if (offset + 4 <= size) {
                    uint32_t fragment_id = *((uint32_t*)(data + offset));
                    offset += 4;
                    if (offset + 4 <= size) {
                        uint32_t new_offset = *((uint32_t*)(data + offset));
                        offset += 4;
                        frame_reassembly_update_fragment(reassembly, fragment_id, new_offset);
                    }
                }
                break;
            }
            case 8: {
                frame_reassembly_validate(reassembly);
                break;
            }
            default:
                break;
        }
    }
    
    frame_reassembly_destroy(reassembly);
}
