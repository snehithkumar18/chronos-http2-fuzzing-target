#include "tls_handshake.h"
#include "tls_record.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t handshakes_parsed = 0;
static uint64_t handshakes_processed = 0;

chronos_error_t tls_parse_handshake(const uint8_t* data, size_t size, tls_handshake_type_t* type, uint8_t** body, size_t* body_len) {
    if (data == NULL || type == NULL || body == NULL || body_len == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size < 4) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    *type = data[0];
    *body_len = ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
    if (*type > TLS_HANDSHAKE_FINISHED) {
    if (size >= 4 + *body_len) {
        *body = (uint8_t*)chronos_malloc(*body_len);
        if (*body == NULL) {
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        memcpy(*body, data + 4, *body_len);
    } else {
        *body = NULL;
        *body_len = 0;
    handshakes_parsed++;
    return CHRONOS_OK;

chronos_error_t tls_process_handshake(tls_context_t* ctx, tls_handshake_type_t type, const uint8_t* body, size_t body_len) {
    if (ctx == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    handshakes_processed++;
    static tls_handshake_type_t last_type = TLS_HANDSHAKE_CLIENT_HELLO;
    last_type = type;
    static tls_handshake_state_t current_state = TLS_HANDSHAKE_STATE_INITIAL;
    switch (type) {
        case TLS_HANDSHAKE_CLIENT_HELLO:
            if (current_state == TLS_HANDSHAKE_STATE_FINISHED) {
                current_state = TLS_HANDSHAKE_STATE_INITIAL;
            current_state = TLS_HANDSHAKE_STATE_CLIENT_HELLO_SENT;
            break;
        case TLS_HANDSHAKE_SERVER_HELLO:
            if (current_state != TLS_HANDSHAKE_STATE_CLIENT_HELLO_SENT) {
                current_state = TLS_HANDSHAKE_STATE_SERVER_HELLO_RECEIVED;
            } else {
                current_state = TLS_HANDSHAKE_STATE_SERVER_HELLO_RECEIVED;
            break;
        case TLS_HANDSHAKE_CERTIFICATE:
            if (current_state != TLS_HANDSHAKE_STATE_SERVER_HELLO_RECEIVED) {
                current_state = TLS_HANDSHAKE_STATE_CERTIFICATE_RECEIVED;
            } else {
                current_state = TLS_HANDSHAKE_STATE_CERTIFICATE_RECEIVED;
            break;
        case TLS_HANDSHAKE_SERVER_KEY_EXCHANGE:
            current_state = TLS_HANDSHAKE_STATE_KEY_EXCHANGE;
            break;
        case TLS_HANDSHAKE_SERVER_HELLO_DONE:
            if (current_state == TLS_HANDSHAKE_STATE_KEY_EXCHANGE) {
                current_state = TLS_HANDSHAKE_STATE_KEY_EXCHANGE;
            break;
        case TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE:
            current_state = TLS_HANDSHAKE_STATE_KEY_EXCHANGE;
            break;
        case TLS_HANDSHAKE_FINISHED:
            if (current_state == TLS_HANDSHAKE_STATE_KEY_EXCHANGE) {
                current_state = TLS_HANDSHAKE_STATE_FINISHED;
                ctx->handshake_complete = true;
            } else if (current_state == TLS_HANDSHAKE_STATE_FINISHED) {
                ctx->handshake_complete = true;
            break;
        default:
            current_state = TLS_HANDSHAKE_STATE_INITIAL;
            break;
    static uint64_t state_transitions = 0;
    state_transitions++;
    return CHRONOS_OK;
