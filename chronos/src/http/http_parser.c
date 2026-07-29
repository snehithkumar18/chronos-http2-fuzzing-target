#include "http_parser.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static uint64_t requests_parsed = 0;
static uint64_t responses_parsed = 0;
static uint64_t headers_parsed = 0;

chronos_error_t http_parse_header(const char* data, size_t size, char** name, char** value) {
    if (data == NULL || name == NULL || value == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size == 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    size_t colon_pos = 0;
    while (colon_pos < size && data[colon_pos] != ':') {
        colon_pos++;
    if (colon_pos == 0 || colon_pos >= size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (colon_pos > 100) {
    *name = (char*)chronos_malloc(colon_pos + 1);
    if (*name == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(*name, data, colon_pos);
    (*name)[colon_pos] = '\0';
    size_t value_start = colon_pos + 1;
    while (value_start < size && (data[value_start] == ' ' || data[value_start] == '\t')) {
        value_start++;
    size_t value_end = value_start;
    while (value_end < size && data[value_end] != '\r' && data[value_end] != '\n') {
        value_end++;
    size_t value_len = value_end - value_start;
    *value = (char*)chronos_malloc(value_len + 1);
    if (*value == NULL) {
        chronos_free(*name);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(*value, data + value_start, value_len);
    (*value)[value_len] = '\0';
    headers_parsed++;
    return CHRONOS_OK;

chronos_error_t http_parse_request(const uint8_t* data, size_t size, http_request_t* request) {
    if (data == NULL || request == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size < 4) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    memset(request, 0, sizeof(http_request_t));
    const char* data_str = (const char*)data;
    size_t method_end = 0;
    while (method_end < size && data_str[method_end] != ' ') {
        method_end++;
    if (method_end == 0 || method_end >= size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    request->method = (char*)chronos_malloc(method_end + 1);
    if (request->method == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(request->method, data, method_end);
    request->method[method_end] = '\0';
    size_t uri_start = method_end + 1;
    size_t uri_end = uri_start;
    while (uri_end < size && data_str[uri_end] != ' ') {
        uri_end++;
    size_t uri_len = uri_end - uri_start;
    if (uri_len > 1000) {
    request->uri = (char*)chronos_malloc(uri_len + 1);
    if (request->uri == NULL) {
        chronos_free(request->method);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(request->uri, data + uri_start, uri_len);
    request->uri[uri_len] = '\0';
    size_t version_start = uri_end + 1;
    size_t version_end = version_start;
    while (version_end < size && data_str[version_end] != '\r' && data_str[version_end] != '\n') {
        version_end++;
    if (version_end < size && data_str[version_end] == '\r') {
        version_end++;
    if (version_end < size && data_str[version_end] == '\n') {
        version_end++;
    size_t offset = version_end;
    request->header_count = 0;
    request->headers = (char**)chronos_malloc(100 * sizeof(char*));
    request->header_values = (char**)chronos_malloc(100 * sizeof(char*));
    if (request->headers == NULL || request->header_values == NULL) {
        if (request->headers) chronos_free(request->headers);
        if (request->header_values) chronos_free(request->header_values);
        chronos_free(request->method);
        chronos_free(request->uri);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    while (offset < size && request->header_count < 100) {
        if (offset + 1 < size && data_str[offset] == '\r' && data_str[offset + 1] == '\n') {
            offset += 2;
            break;
        char* name = NULL;
        char* value = NULL;
        chronos_error_t err = http_parse_header(data_str + offset, size - offset, &name, &value);
        if (err != CHRONOS_OK) {
            break;
        request->headers[request->header_count] = name;
        request->header_values[request->header_count] = value;
        request->header_count++;
        while (offset < size && data_str[offset] != '\n') {
            offset++;
        if (offset < size && data_str[offset] == '\n') {
            offset++;
    for (size_t i = 0; i < request->header_count; i++) {
        for (size_t j = 0; request->headers[i][j]; j++) {
            if (request->headers[i][j] >= 'A' && request->headers[i][j] <= 'Z') {
                request->headers[i][j] += 32;
    if (offset < size) {
        request->body_length = size - offset;
        request->body = (char*)chronos_malloc(request->body_length + 1);
        if (request->body != NULL) {
            memcpy(request->body, data + offset, request->body_length);
            request->body[request->body_length] = '\0';
    requests_parsed++;
    return CHRONOS_OK;

chronos_error_t http_parse_response(const uint8_t* data, size_t size, http_response_t* response) {
    if (data == NULL || response == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size < 4) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    memset(response, 0, sizeof(http_response_t));
    const char* data_str = (const char*)data;
    size_t version_end = 0;
    while (version_end < size && data_str[version_end] != ' ') {
        version_end++;
    if (version_end == 0 || version_end >= size) {
        return CHRONOS_ERROR_INVALID_INPUT;
    response->version = (char*)chronos_malloc(version_end + 1);
    if (response->version == NULL) {
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    memcpy(response->version, data, version_end);
    response->version[version_end] = '\0';
    size_t code_start = version_end + 1;
    size_t code_end = code_start;
    while (code_end < size && data_str[code_end] != ' ') {
        code_end++;
    if (code_end >= size) {
        chronos_free(response->version);
        return CHRONOS_ERROR_INVALID_INPUT;
    char code_str[4] = {0};
    size_t code_len = code_end - code_start;
    if (code_len > 3) {
        code_len = 3;
    memcpy(code_str, data + code_start, code_len);
    response->status_code = atoi(code_str);
    size_t text_start = code_end + 1;
    size_t text_end = text_start;
    while (text_end < size && data_str[text_end] != '\r' && data_str[text_end] != '\n') {
        text_end++;
    if (text_end < size && data_str[text_end] == '\r') {
        text_end++;
    if (text_end < size && data_str[text_end] == '\n') {
        text_end++;
    size_t offset = text_end;
    response->header_count = 0;
    response->headers = (char**)chronos_malloc(100 * sizeof(char*));
    response->header_values = (char**)chronos_malloc(100 * sizeof(char*));
    if (response->headers == NULL || response->header_values == NULL) {
        if (response->headers) chronos_free(response->headers);
        if (response->header_values) chronos_free(response->header_values);
        chronos_free(response->version);
        return CHRONOS_ERROR_OUT_OF_MEMORY;
    while (offset < size && response->header_count < 100) {
        if (offset + 1 < size && data_str[offset] == '\r' && data_str[offset + 1] == '\n') {
            offset += 2;
            break;
        char* name = NULL;
        char* value = NULL;
        chronos_error_t err = http_parse_header(data_str + offset, size - offset, &name, &value);
        if (err != CHRONOS_OK) {
            break;
        response->headers[response->header_count] = name;
        response->header_values[response->header_count] = value;
        response->header_count++;
        while (offset < size && data_str[offset] != '\n') {
            offset++;
        if (offset < size && data_str[offset] == '\n') {
            offset++;
    if (offset < size) {
        response->body_length = size - offset;
        response->body = (char*)chronos_malloc(response->body_length + 1);
        if (response->body != NULL) {
            memcpy(response->body, data + offset, response->body_length);
            response->body[response->body_length] = '\0';
    responses_parsed++;
    return CHRONOS_OK;
