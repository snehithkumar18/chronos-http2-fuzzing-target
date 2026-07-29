#ifndef CHRONOS_H2_STREAM_H
#define CHRONOS_H2_STREAM_H

#include <stdint.h>
#include "../common.h"

typedef enum {
    H2_STREAM_IDLE = 0,
    H2_STREAM_RESERVED_LOCAL = 1,
    H2_STREAM_RESERVED_REMOTE = 2,
    H2_STREAM_OPEN = 3,
    H2_STREAM_HALF_CLOSED_LOCAL = 4,
    H2_STREAM_HALF_CLOSED_REMOTE = 5,
    H2_STREAM_CLOSED = 6,
} h2_stream_state_t;

typedef struct h2_stream h2_stream_t;
typedef struct h2_connection h2_connection_t;

struct h2_stream {
    uint32_t stream_id;
    h2_stream_state_t state;
    int32_t send_window;
    int32_t recv_window;
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    h2_connection_t* connection;
    h2_stream_t* next;
};

typedef struct h2_connection h2_connection_t;

h2_stream_t* h2_stream_create(h2_connection_t* conn, uint32_t stream_id);
void h2_stream_destroy(h2_stream_t* stream);
chronos_error_t h2_stream_process_frame(h2_stream_t* stream, const h2_frame_t* frame);
chronos_error_t h2_stream_transition_state(h2_stream_t* stream, h2_stream_state_t new_state);
h2_stream_t* h2_stream_lookup(h2_connection_t* conn, uint32_t stream_id);

