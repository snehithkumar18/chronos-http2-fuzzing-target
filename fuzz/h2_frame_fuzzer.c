#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/h2_frame.h"
#include "../src/protocol/h2_connection.h"
#include "../src/protocol/h2_stream.h"
#include "../src/protocol/h2_settings.h"
#include "../src/protocol/h2_push.h"
#include "../src/net/buffer.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 13) return;
    
    size_t offset = 0;
    
    uint32_t stream_id = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t buffer_size = *((uint32_t*)(data + offset));
    offset += 4;
    
    if (buffer_size == 0) buffer_size = 4096;
    if (buffer_size > 1000000) buffer_size = 4096;
    
    // Create connection
    h2_connection_t *conn = h2_connection_create();
    if (!conn) return;
    
    // Create stream
    h2_stream_t *stream = h2_stream_create(conn, stream_id);
    if (!stream) {
        h2_connection_destroy(conn);
        return;
    }
    
    // Parse frame
    h2_frame_t frame;
    if (h2_parse_frame(data, size, &frame) == CHRONOS_OK) {
        // Process frame on connection
        h2_process_frame(conn, &frame);
        
        // Process frame on stream
        h2_stream_process_frame(stream, &frame);
        
        if (frame.length > 0 && frame.payload) {
            ring_buffer_t *buffer = ring_buffer_create(buffer_size);
            if (buffer) {
                ring_buffer_write(buffer, frame.payload, frame.length);
                uint8_t read_data[buffer_size];
                size_t bytes_read;
                ring_buffer_read(buffer, read_data, buffer_size, &bytes_read);
                ring_buffer_destroy(buffer);
            }
        }
        
        if (frame.type == H2_FRAME_SETTINGS && frame.length > 0) {
            h2_settings_t settings;
            h2_parse_settings(frame.payload, frame.length, &settings);
            h2_validate_settings(&settings);
        }
        
        if (frame.type == H2_FRAME_PUSH_PROMISE) {
            h2_push_promise_t push;
            h2_parse_push_promise(&frame, &push);
            h2_push_promise_cleanup(&push);
        }
    }
    
    h2_frame_cleanup(&frame);
    h2_stream_destroy(stream);
    h2_connection_destroy(conn);
}
