#ifndef CHRONOS_H2_FRAME_H
#define CHRONOS_H2_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef enum {
    H2_FRAME_DATA = 0x0,
    H2_FRAME_HEADERS = 0x1,
    H2_FRAME_PRIORITY = 0x2,
    H2_FRAME_RST_STREAM = 0x3,
    H2_FRAME_SETTINGS = 0x4,
    H2_FRAME_PUSH_PROMISE = 0x5,
    H2_FRAME_PING = 0x6,
    H2_FRAME_GOAWAY = 0x7,
    H2_FRAME_WINDOW_UPDATE = 0x8,
    H2_FRAME_CONTINUATION = 0x9,
} h2_frame_type_t;

typedef struct {
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id;
    uint8_t* payload;
} h2_frame_t;

chronos_error_t h2_parse_frame(const uint8_t* data, size_t size, h2_frame_t* frame);
void h2_frame_init(h2_frame_t* frame);
void h2_frame_cleanup(h2_frame_t* frame);

