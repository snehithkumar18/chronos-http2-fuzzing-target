#ifndef CHRONOS_H2_PUSH_H
#define CHRONOS_H2_PUSH_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "h2_frame.h"

typedef struct {
    uint32_t promised_stream_id;
    uint8_t* header_block;
    size_t header_block_length;
} h2_push_promise_t;

chronos_error_t h2_parse_push_promise(const h2_frame_t* frame, h2_push_promise_t* push);
void h2_push_promise_cleanup(h2_push_promise_t* push);

