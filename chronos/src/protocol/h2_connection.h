#ifndef CHRONOS_H2_CONNECTION_H
#define CHRONOS_H2_CONNECTION_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "h2_stream.h"
#include "h2_frame.h"

typedef struct h2_connection {
    uint32_t last_stream_id;
    int32_t send_window;
    int32_t recv_window;
    h2_stream_t* streams;
    uint32_t stream_count;
    uint8_t settings[16];
    bool settings_ack_pending;
} h2_connection_t;

h2_connection_t* h2_connection_create(void);
void h2_connection_destroy(h2_connection_t* conn);
chronos_error_t h2_process_frame(h2_connection_t* conn, const h2_frame_t* frame);
chronos_error_t h2_connection_add_stream(h2_connection_t* conn, h2_stream_t* stream);
chronos_error_t h2_update_flow_window(h2_connection_t* conn, int32_t increment);

