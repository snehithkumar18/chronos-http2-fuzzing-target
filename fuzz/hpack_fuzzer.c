#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/hpack/hpack_decoder.h"
#include "../src/hpack/hpack_encoder.h"
#include "../src/hpack/hpack_table.h"
#include "../src/hpack/huffman.h"
#include "../src/utils/base64.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) return;
    
    size_t offset = 0;
    
    uint32_t table_size = *((uint32_t*)(data + offset));
    offset += 4;
    
    if (table_size == 0) table_size = 4096;
    if (table_size > 1000000) table_size = 4096;
    
    // Create HPACK context
    hpack_context_t *ctx = hpack_context_create(table_size);
    if (!ctx) return;
    
    // Decode headers (Bugs #5, #17, #19, #28)
    hpack_header_t headers[100];
    int count = hpack_decode_headers(ctx, data, size, headers, 100);
    
    if (count > 0) {
        uint32_t result;
        size_t consumed;
        hpack_decode_integer(data, size, 7, &result, &consumed);
        
        uint8_t output[8192];
        for (int i = 0; i < count && i < 10; i++) {
            if (headers[i].name && headers[i].value) {
                hpack_encode_header(ctx, headers[i].name, headers[i].name_len, 
                                   headers[i].value, headers[i].value_len, output, 8192);
            }
        }
        
        for (int i = 0; i < count && i < 10; i++) {
            if (headers[i].value && headers[i].value_len > 0) {
                char decoded[8192];
                huffman_decode((uint8_t*)headers[i].value, headers[i].value_len, decoded, 8192);
            }
        }
        
        hpack_table_resize(ctx->table, 2048);
        hpack_table_resize(ctx->table, 4096);
        
        for (int i = 0; i < count && i < 5; i++) {
            if (headers[i].value && headers[i].value_len > 0) {
                uint8_t decoded[8192];
                chronos_base64_decode((char*)headers[i].value, headers[i].value_len, decoded, 8192);
            }
        }
    }
    
    hpack_context_destroy(ctx);
}
