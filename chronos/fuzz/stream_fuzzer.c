#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../src/protocol/h2_frame.h"
#include "../src/protocol/h2_connection.h"
#include "../src/protocol/h2_stream.h"
#include "../src/protocol/h2_settings.h"
#include "../src/net/epoll.h"
#include "../src/net/socket.h"

static void event_callback(socket_t *sock, void *user_data) {
    // This callback can be invoked on sockets that are being destroyed
    // The callback mechanism creates a reentrancy issue
}

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 13) return;
    
    size_t offset = 0;
    
    uint32_t num_streams = *((uint32_t*)(data + offset));
    offset += 4;
    uint32_t max_events = *((uint32_t*)(data + offset));
    offset += 4;
    
    if (num_streams == 0) num_streams = 10;
    if (num_streams > 100) num_streams = 10;
    if (max_events == 0) max_events = 20;
    if (max_events > 1000) max_events = 20;
    
    // Create connection
    h2_connection_t *conn = h2_connection_create();
    if (!conn) return;
    
    // Create multiple streams
    h2_stream_t *streams[num_streams];
    for (uint32_t i = 0; i < num_streams; i++) {
        streams[i] = h2_stream_create(conn, i + 1);
        if (streams[i]) {
            h2_connection_add_stream(conn, streams[i]);
        }
    }
    
    // Create epoll context (Bug #24)
    epoll_context_t *epoll_ctx = epoll_context_create();
    if (epoll_ctx) {
        socket_t *sock = socket_create(1);
        if (sock) {
            epoll_add_socket(epoll_ctx, sock, event_callback, NULL);
            epoll_process_events(epoll_ctx, 100);
            epoll_remove_socket(epoll_ctx, sock);
            socket_destroy(sock);
        }
        epoll_context_destroy(epoll_ctx);
    }
    
    // Process multiple frames for state transitions (Bugs #14, #15, #26, #27)
    int frame_count = 0;
    
    while (offset + 9 <= size && frame_count < max_events) {
        h2_frame_t frame;
        if (h2_parse_frame(data + offset, size - offset, &frame) == CHRONOS_OK) {
            // Process on all streams (Bug #13, #26)
            for (uint32_t i = 0; i < num_streams; i++) {
                if (streams[i]) {
                    h2_stream_process_frame(streams[i], &frame);
                    
                    if (frame.type == H2_FRAME_RST_STREAM) {
                        h2_stream_transition_state(streams[i], H2_STREAM_CLOSED);
                    }
                }
            }
            
            // Process on connection (Bugs #13, #15, #16, #27)
            h2_process_frame(conn, &frame);
            
            if (frame.type == H2_FRAME_WINDOW_UPDATE && offset + 4 <= size) {
                uint32_t window_update = *((uint32_t*)(data + offset));
                offset += 4;
                h2_update_flow_window(conn, window_update);
            }
            
            if (frame.type == H2_FRAME_SETTINGS) {
                h2_settings_t settings;
                h2_parse_settings(frame.payload, frame.length, &settings);
            }
            
            h2_frame_cleanup(&frame);
            frame_count++;
            offset += 9 + frame.length;
        } else {
            break;
        }
    }
    
    // Clean up streams (Bug #26: Complex lifecycle UAF)
    for (uint32_t i = 0; i < num_streams; i++) {
        if (streams[i]) {
            h2_stream_destroy(streams[i]);
        }
    }
    
    h2_connection_destroy(conn);
}
