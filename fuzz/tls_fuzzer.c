#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/tls/tls_record.h"
#include "../src/tls/tls_handshake.h"
#include "../src/tls/tls_crypto.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return;
    
    size_t offset = 0;
    
    uint32_t cipher_suite = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t mac_key_size = data[offset++];
    
    if (mac_key_size == 0) mac_key_size = 16;
    if (mac_key_size > 64) mac_key_size = 16;
    
    // Create TLS context
    tls_context_t *ctx = tls_context_create();
    if (!ctx) return;
    
    // Parse record (Bug #8)
    tls_record_t record;
    if (tls_parse_record(data, size, &record) == CHRONOS_OK) {
        // Process record (Bugs #20, #21)
        tls_process_record(ctx, &record);
        
        // Parse handshake if content type is handshake (Bugs #20, #29)
        if (record.content_type == TLS_CONTENT_TYPE_HANDSHAKE && record.data) {
            tls_handshake_type_t type;
            uint8_t *body = NULL;
            size_t body_len = 0;
            
            if (tls_parse_handshake(record.data, record.length, &type, &body, &body_len) == CHRONOS_OK) {
                // Process handshake (Bugs #20, #29)
                tls_process_handshake(ctx, type, body, body_len);
                
                if (type == TLS_HANDSHAKE_CLIENT_HELLO && body_len >= 2) {
                    tls_select_cipher_suite((tls_cipher_suite_t)cipher_suite);
                }
                
                if (body) {
                    free(body);
                }
            }
        }
        
        if (record.length > 0 && record.data) {
            uint8_t mac[32];
            uint8_t key[16] = {0};
            tls_calculate_mac(record.data, record.length, key, 16, mac, 32);
        }
        
        if (record.length > 0 && record.data) {
            uint8_t encrypted[16384];
            uint8_t decrypted[16384];
            tls_encrypt(record.data, record.length, encrypted, 16384);
            tls_decrypt(encrypted, record.length, decrypted, 16384);
        }
    }
    
    if (record.data) {
        free(record.data);
    }
    
    tls_context_destroy(ctx);
}
