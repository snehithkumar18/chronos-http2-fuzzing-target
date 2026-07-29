#ifndef CHRONOS_SOCKET_H
#define CHRONOS_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef struct {
    int fd;
    bool connected;
    uint8_t* read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_capacity;
} socket_t;

socket_t* socket_create(int fd);
void socket_destroy(socket_t* sock);
chronos_error_t socket_read(socket_t* sock, uint8_t* data, size_t size, size_t* bytes_read);
chronos_error_t socket_write(socket_t* sock, const uint8_t* data, size_t size);

