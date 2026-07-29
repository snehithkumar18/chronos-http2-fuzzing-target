#ifndef CHRONOS_TLS_RECORD_H
#define CHRONOS_TLS_RECORD_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef enum {
    TLS_CONTENT_TYPE_CHANGE_CIPHER_SPEC = 20,
    TLS_CONTENT_TYPE_ALERT = 21,
    TLS_CONTENT_TYPE_HANDSHAKE = 22,
    TLS_CONTENT_TYPE_APPLICATION_DATA = 23,
} tls_content_type_t;

typedef struct {
    uint8_t content_type;
    uint16_t version;
    uint16_t length;
    uint8_t* data;
} tls_record_t;

typedef struct tls_context tls_context_t;

tls_context_t* tls_context_create(void);
void tls_context_destroy(tls_context_t* ctx);
chronos_error_t tls_parse_record(const uint8_t* data, size_t size, tls_record_t* record);
chronos_error_t tls_process_record(tls_context_t* ctx, const tls_record_t* record);

