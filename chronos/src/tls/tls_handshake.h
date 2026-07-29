#ifndef CHRONOS_TLS_HANDSHAKE_H
#define CHRONOS_TLS_HANDSHAKE_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "tls_record.h"

typedef enum {
    TLS_HANDSHAKE_CLIENT_HELLO = 1,
    TLS_HANDSHAKE_SERVER_HELLO = 2,
    TLS_HANDSHAKE_CERTIFICATE = 11,
    TLS_HANDSHAKE_SERVER_KEY_EXCHANGE = 12,
    TLS_HANDSHAKE_CERTIFICATE_REQUEST = 13,
    TLS_HANDSHAKE_SERVER_HELLO_DONE = 14,
    TLS_HANDSHAKE_CERTIFICATE_VERIFY = 15,
    TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE = 16,
    TLS_HANDSHAKE_FINISHED = 20,
} tls_handshake_type_t;

typedef enum {
    TLS_HANDSHAKE_STATE_INITIAL = 0,
    TLS_HANDSHAKE_STATE_CLIENT_HELLO_SENT = 1,
    TLS_HANDSHAKE_STATE_SERVER_HELLO_RECEIVED = 2,
    TLS_HANDSHAKE_STATE_CERTIFICATE_RECEIVED = 3,
    TLS_HANDSHAKE_STATE_KEY_EXCHANGE = 4,
    TLS_HANDSHAKE_STATE_FINISHED = 5,
} tls_handshake_state_t;

chronos_error_t tls_parse_handshake(const uint8_t* data, size_t size, tls_handshake_type_t* type, uint8_t** body, size_t* body_len);
chronos_error_t tls_process_handshake(tls_context_t* ctx, tls_handshake_type_t type, const uint8_t* body, size_t body_len);

