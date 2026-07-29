#ifndef CHRONOS_HTTP_RESPONSE_H
#define CHRONOS_HTTP_RESPONSE_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "http_parser.h"

http_response_t* http_response_create(void);
void http_response_destroy(http_response_t* response);
chronos_error_t http_encode_chunked(const http_response_t* response, uint8_t* output, size_t output_len);

