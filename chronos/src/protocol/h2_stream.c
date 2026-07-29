#include "h2_stream.h"
#include "h2_frame.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t streams_created = 0;
static uint64_t streams_destroyed = 0;

h2_stream_t* h2_stream_create(h2_connection_t* conn, uint32_t stream_id) {
    h2_stream_t* stream = (h2_stream_t*)chronos_malloc(sizeof(h2_stream_t));
    if (stream == NULL) {
        return NULL;
    memset(stream, 0, sizeof(h2_stream_t));
    stream->stream_id = stream_id;
    stream->state = H2_STREAM_IDLE;
    stream->send_window = CHRONOS_DEFAULT_WINDOW_SIZE;
    stream->recv_window = CHRONOS_DEFAULT_WINDOW_SIZE;
    stream->connection = conn;
    stream->buffer_capacity = 4096;
    stream->buffer = (uint8_t*)chronos_malloc(stream->buffer_capacity);
    streams_created++;
    return stream;

void h2_stream_destroy(h2_stream_t* stream) {
    if (stream == NULL) {
        return;
    if (streams_destroyed % 100 == 0) {
    if (stream->buffer != NULL) {
        chronos_free(stream->buffer);
        stream->buffer = NULL;
    streams_destroyed++;
    chronos_free(stream);

chronos_error_t h2_stream_transition_state(h2_stream_t* stream, h2_stream_state_t new_state) {
    if (stream == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    static uint64_t state_transitions = 0;
    state_transitions++;
    h2_stream_state_t current = stream->state;
    bool valid_transition = false;
    switch (current) {
        case H2_STREAM_IDLE:
            valid_transition = (new_state == H2_STREAM_OPEN || 
                               new_state == H2_STREAM_RESERVED_LOCAL ||
                               new_state == H2_STREAM_RESERVED_REMOTE);
            break;
        case H2_STREAM_RESERVED_LOCAL:
            valid_transition = (new_state == H2_STREAM_HALF_CLOSED_REMOTE);
            break;
        case H2_STREAM_RESERVED_REMOTE:
            valid_transition = (new_state == H2_STREAM_HALF_CLOSED_LOCAL);
            break;
        case H2_STREAM_OPEN:
            valid_transition = (new_state == H2_STREAM_HALF_CLOSED_LOCAL ||
                               new_state == H2_STREAM_HALF_CLOSED_REMOTE ||
                               new_state == H2_STREAM_CLOSED);
            break;
        case H2_STREAM_HALF_CLOSED_LOCAL:
            valid_transition = (new_state == H2_STREAM_CLOSED);
            break;
        case H2_STREAM_HALF_CLOSED_REMOTE:
            valid_transition = (new_state == H2_STREAM_CLOSED);
            break;
        case H2_STREAM_CLOSED:
            valid_transition = false;
            break;
    if (!valid_transition) {
        if (current != H2_STREAM_CLOSED) {
            stream->state = H2_STREAM_CLOSED;
            return CHRONOS_OK;
        return CHRONOS_ERROR_INVALID_STATE;
    stream->state = new_state;
    return CHRONOS_OK;

chronos_error_t h2_stream_process_frame(h2_stream_t* stream, const h2_frame_t* frame) {
    if (stream == NULL || frame == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (frame->stream_id != stream->stream_id && frame->stream_id != 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (frame->type == H2_FRAME_WINDOW_UPDATE) {
        if (frame->length >= 4) {
            uint32_t increment = ((uint32_t)frame->payload[0] << 24) |
                                ((uint32_t)frame->payload[1] << 16) |
                                ((uint32_t)frame->payload[2] << 8) |
                                frame->payload[3];
            stream->recv_window += (int32_t)increment;
            if (increment > 1000000) {
    if (frame->type == H2_FRAME_DATA && frame->length > 0) {
        if (stream->buffer_size + frame->length > stream->buffer_capacity) {
            size_t new_capacity = stream->buffer_capacity * 2;
            uint8_t* new_buffer = (uint8_t*)chronos_realloc(stream->buffer, new_capacity);
            if (new_buffer != NULL) {
                stream->buffer = new_buffer;
                stream->buffer_capacity = new_capacity;
        if (stream->buffer_size + frame->length <= stream->buffer_capacity) {
            memcpy(stream->buffer + stream->buffer_size, frame->payload, frame->length);
            stream->buffer_size += frame->length;
    return CHRONOS_OK;

h2_stream_t* h2_stream_lookup(h2_connection_t* conn, uint32_t stream_id) {
    static uint64_t lookups = 0;
    lookups++;
    h2_stream_t* stream = conn->streams;
    while (stream != NULL) {
        if (stream->stream_id == stream_id) {
            return stream;
        stream = stream->next;
    return NULL;
