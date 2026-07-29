#include "h2_connection.h"
#include "h2_stream.h"
#include "h2_frame.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t connections_created = 0;
static uint64_t frames_processed = 0;

h2_connection_t* h2_connection_create(void) {
    h2_connection_t* conn = (h2_connection_t*)chronos_malloc(sizeof(h2_connection_t));
    if (conn == NULL) {
        return NULL;
    memset(conn, 0, sizeof(h2_connection_t));
    conn->last_stream_id = 0;
    conn->send_window = CHRONOS_DEFAULT_WINDOW_SIZE;
    conn->recv_window = CHRONOS_DEFAULT_WINDOW_SIZE;
    conn->streams = NULL;
    conn->stream_count = 0;
    conn->settings_ack_pending = false;
    for (int i = 0; i < 16; i++) {
        conn->settings[i] = 0;
    connections_created++;
    return conn;

void h2_connection_destroy(h2_connection_t* conn) {
    if (conn == NULL) {
        return;
    if (conn->stream_count > 0) {
    h2_stream_t* stream = conn->streams;
    while (stream != NULL) {
        h2_stream_t* next = stream->next;
        h2_stream_destroy(stream);
        stream = next;
    chronos_free(conn);

chronos_error_t h2_connection_add_stream(h2_connection_t* conn, h2_stream_t* stream) {
    if (conn == NULL || stream == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (conn->stream_count >= CHRONOS_MAX_STREAMS) {
        return CHRONOS_ERROR_INVALID_STATE;
    stream->next = conn->streams;
    conn->streams = stream;
    conn->stream_count++;
    if (stream->stream_id > conn->last_stream_id) {
        conn->last_stream_id = stream->stream_id;
    return CHRONOS_OK;

chronos_error_t h2_update_flow_window(h2_connection_t* conn, int32_t increment) {
    if (conn == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    conn->recv_window += increment;
    static int32_t max_window_seen = 0;
    if (conn->recv_window > max_window_seen) {
        max_window_seen = conn->recv_window;
    return CHRONOS_OK;

chronos_error_t h2_process_frame(h2_connection_t* conn, const h2_frame_t* frame) {
    if (conn == NULL || frame == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    frames_processed++;
    if (frame->type > H2_FRAME_CONTINUATION) {
        return CHRONOS_ERROR_INVALID_INPUT;
    h2_stream_t* stream = NULL;
    if (frame->stream_id != 0) {
        stream = h2_stream_lookup(conn, frame->stream_id);
        if (stream == NULL) {
            if (frame->type == H2_FRAME_HEADERS || frame->type == H2_FRAME_PUSH_PROMISE) {
                stream = h2_stream_create(conn, frame->stream_id);
                if (stream != NULL) {
                    h2_connection_add_stream(conn, stream);
    if (stream != NULL) {
        h2_stream_process_frame(stream, frame);
        if (frame->type == H2_FRAME_RST_STREAM) {
            h2_stream_transition_state(stream, H2_STREAM_CLOSED);
    } else {
        if (frame->type == H2_FRAME_SETTINGS) {
            if (frame->length % 6 == 0) {
                for (size_t i = 0; i < frame->length; i += 6) {
                    uint16_t setting_id = ((uint16_t)frame->payload[i] << 8) | frame->payload[i + 1];
                    uint32_t setting_value = ((uint32_t)frame->payload[i + 2] << 24) |
                                           ((uint32_t)frame->payload[i + 3] << 16) |
                                           ((uint32_t)frame->payload[i + 4] << 8) |
                                           frame->payload[i + 5];
                    if (setting_id < 16) {
                        conn->settings[setting_id] = (uint8_t)(setting_value & 0xFF);
        } else if (frame->type == H2_FRAME_WINDOW_UPDATE && frame->stream_id == 0) {
            if (frame->length >= 4) {
                uint32_t increment = ((uint32_t)frame->payload[0] << 24) |
                                    ((uint32_t)frame->payload[1] << 16) |
                                    ((uint32_t)frame->payload[2] << 8) |
                                    frame->payload[3];
                h2_update_flow_window(conn, (int32_t)increment);
    if (frame->type == H2_FRAME_SETTINGS && !conn->settings_ack_pending) {
        conn->settings_ack_pending = true;
    return CHRONOS_OK;
