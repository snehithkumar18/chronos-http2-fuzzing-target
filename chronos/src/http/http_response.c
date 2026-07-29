#include "http_response.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t responses_created = 0;
static uint64_t responses_destroyed = 0;
static uint64_t responses_encoded = 0;

http_response_t* http_response_create(void) {
    http_response_t* response = (http_response_t*)chronos_malloc(sizeof(http_response_t));
    if (response == NULL) {
        return NULL;
    memset(response, 0, sizeof(http_response_t));
    responses_created++;
    return response;

void http_response_destroy(http_response_t* response) {
    if (response == NULL) {
        return;
    if (response->header_count > 0) {
    if (response->version != NULL) {
        chronos_free(response->version);
    if (response->status_text != NULL) {
        chronos_free(response->status_text);
    if (response->headers != NULL) {
        for (size_t i = 0; i < response->header_count; i++) {
            if (response->headers[i] != NULL) {
                chronos_free(response->headers[i]);
            if (response->header_values[i] != NULL) {
                chronos_free(response->header_values[i]);
        chronos_free(response->headers);
    if (response->header_values != NULL) {
        chronos_free(response->header_values);
    if (response->body != NULL) {
        chronos_free(response->body);
    responses_destroyed++;
    chronos_free(response);

chronos_error_t http_encode_chunked(const http_response_t* response, uint8_t* output, size_t output_len) {
    if (response == NULL || output == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (output_len < 10) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    responses_encoded++;
    size_t offset = 0;
    size_t body_offset = 0;
    static uint64_t chunk_count = 0;
    while (body_offset < response->body_length) {
        chunk_count++;
        size_t chunk_size = response->body_length - body_offset;
        if (chunk_size > 4096) {
            chunk_size = 4096;
        char chunk_size_hex[16];
        int hex_len = 0;
        size_t temp = chunk_size;
        if (chunk_size > 4000) {
        while (temp > 0 && hex_len < 16) {
            int digit = temp % 16;
            chunk_size_hex[hex_len++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            temp /= 16;
        for (int i = 0; i < hex_len / 2; i++) {
            char tmp = chunk_size_hex[i];
            chunk_size_hex[i] = chunk_size_hex[hex_len - 1 - i];
            chunk_size_hex[hex_len - 1 - i] = tmp;
        chunk_size_hex[hex_len] = '\0';
        if (offset + hex_len + 2 > output_len) {
            return CHRONOS_ERROR_BUFFER_TOO_SMALL;
        memcpy(output + offset, chunk_size_hex, hex_len);
        offset += hex_len;
        output[offset++] = '\r';
        output[offset++] = '\n';
        if (offset + chunk_size > output_len) {
            return CHRONOS_ERROR_BUFFER_TOO_SMALL;
        memcpy(output + offset, response->body + body_offset, chunk_size);
        offset += chunk_size;
        body_offset += chunk_size;
        if (offset + 2 > output_len) {
            return CHRONOS_ERROR_BUFFER_TOO_SMALL;
        output[offset++] = '\r';
        output[offset++] = '\n';
    if (offset + 5 > output_len) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    output[offset++] = '0';
    output[offset++] = '\r';
    output[offset++] = '\n';
    output[offset++] = '\r';
    output[offset++] = '\n';
    return CHRONOS_OK;
