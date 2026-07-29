#ifndef CHRONOS_HTTP_PARSER_H
#define CHRONOS_HTTP_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct {
    char* method;
    char* uri;
    char* version;
    char** headers;
    char** header_values;
    size_t header_count;
    char* body;
    size_t body_length;
} http_request_t;

typedef struct {
    char* version;
    int status_code;
    char* status_text;
    char** headers;
    char** header_values;
    size_t header_count;
    char* body;
    size_t body_length;
} http_response_t;

chronos_error_t http_parse_request(const uint8_t* data, size_t size, http_request_t* request);
chronos_error_t http_parse_response(const uint8_t* data, size_t size, http_response_t* response);
chronos_error_t http_parse_header(const char* data, size_t size, char** name, char** value);

