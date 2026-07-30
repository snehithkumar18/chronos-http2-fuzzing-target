#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/http/http_parser.h"
#include "../src/http/http_request.h"
#include "../src/http/http_response.h"
#include "../src/utils/base64.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return;
    
    size_t offset = 0;
    
    uint32_t max_headers = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_body_size = *((uint32_t*)(data + offset));
    offset += 4;
    
    if (max_headers == 0) max_headers = 100;
    if (max_headers > 10000) max_headers = 10000;
    if (max_body_size == 0) max_body_size = 65535;
    if (max_body_size > 1000000) max_body_size = 65535;
    
    // Parse HTTP request (Bugs #10, #11, #22, #30)
    http_request_t *request = http_request_create();
    if (!request) return;
    
    if (http_parse_request(data, size, request) == CHRONOS_OK) {
        // Process request (Bugs #22, #30)
        http_process_request(request);
        
        // Validate request (Bug #22)
        http_validate_request(request);
        
        for (size_t i = 0; i < request->header_count && i < 10; i++) {
            if (request->headers[i] && request->header_values[i]) {
                if (request->header_values[i][0] != 0) {
                    uint8_t decoded[8192];
                    chronos_base64_decode(request->header_values[i], 
                                       strlen(request->header_values[i]), 
                                       decoded, 8192);
                }
            }
        }
        
        http_response_t *response = http_response_create();
        if (response) {
            response->body = request->body;
            response->body_length = request->body_length;
            
            uint8_t output[16384];
            http_encode_chunked(response, output, 16384);
            
            http_response_destroy(response);
        }
    }
    
    // Parse HTTP response (Bug #22)
    http_response_t *response = http_response_create();
    if (response) {
        http_parse_response(data, size, response);
        http_response_destroy(response);
    }
    
    http_request_destroy(request);
}
