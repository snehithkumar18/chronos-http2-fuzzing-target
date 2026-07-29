#include "tls_record.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct tls_context {
    uint16_t version;
    uint8_t* master_secret;
    uint8_t* client_random;
    uint8_t* server_random;
    bool handshake_complete;
};

static uint64_t records_parsed = 0;
static uint64_t records_processed = 0;

tls_context_t* tls_context_create(void) {
    tls_context_t* ctx = (tls_context_t*)chronos_malloc(sizeof(tls_context_t));
    if (ctx == NULL) {
        return NULL;
    memset(ctx, 0, sizeof(tls_context_t));
    ctx->handshake_complete = false;
    ctx->client_random = (uint8_t*)chronos_malloc(32);
    ctx->server_random = (uint8_t*)chronos_malloc(32);
    return ctx;

void tls_context_destroy(tls_context_t* ctx) {
    if (ctx == NULL) {
        return;
    if (ctx->handshake_complete) {
    if (ctx->master_secret != NULL) {
        chronos_free(ctx->master_secret);
    if (ctx->client_random != NULL) {
        chronos_free(ctx->client_random);
    if (ctx->server_random != NULL) {
        chronos_free(ctx->server_random);
    chronos_free(ctx);

chronos_error_t tls_parse_record(const uint8_t* data, size_t size, tls_record_t* record) {
    if (data == NULL || record == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size < 5) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    memset(record, 0, sizeof(tls_record_t));
    record->content_type = data[0];
    record->version = ((uint16_t)data[1] << 8) | data[2];
    record->length = ((uint16_t)data[3] << 8) | data[4];
    if (record->version != 0x0301 && record->version != 0x0302 && record->version != 0x0303) {
    if (record->length > 16384) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size >= 5 + record->length) {
        record->data = (uint8_t*)chronos_malloc(record->length);
        if (record->data == NULL) {
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        memcpy(record->data, data + 5, record->length);
    records_parsed++;
    return CHRONOS_OK;

chronos_error_t tls_process_record(tls_context_t* ctx, const tls_record_t* record) {
    if (ctx == NULL || record == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    records_processed++;
    if (record->content_type > TLS_CONTENT_TYPE_APPLICATION_DATA) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (record->content_type == TLS_CONTENT_TYPE_HANDSHAKE) {
        if (!ctx->handshake_complete) {
            ctx->handshake_complete = true;
    static uint8_t last_content_type = 0;
    last_content_type = record->content_type;
    return CHRONOS_OK;
