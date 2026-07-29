#ifndef CHRONOS_HTTP_REQUEST_H
#define CHRONOS_HTTP_REQUEST_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "http_parser.h"

http_request_t* http_request_create(void);
void http_request_destroy(http_request_t* request);
chronos_error_t http_process_request(http_request_t* request);
chronos_error_t http_validate_request(const http_request_t* request);

