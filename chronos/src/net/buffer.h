#ifndef CHRONOS_BUFFER_H
#define CHRONOS_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t size;
    size_t read_pos;
    size_t write_pos;
} ring_buffer_t;

ring_buffer_t* ring_buffer_create(size_t capacity);
void ring_buffer_destroy(ring_buffer_t* buffer);
chronos_error_t ring_buffer_write(ring_buffer_t* buffer, const uint8_t* data, size_t size);
chronos_error_t ring_buffer_read(ring_buffer_t* buffer, uint8_t* data, size_t size, size_t* bytes_read);

