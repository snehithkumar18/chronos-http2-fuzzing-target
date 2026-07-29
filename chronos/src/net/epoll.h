#ifndef CHRONOS_EPOLL_H
#define CHRONOS_EPOLL_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"
#include "socket.h"

typedef void (*event_callback_t)(socket_t* sock, void* user_data);

typedef struct {
    int epoll_fd;
    socket_t** sockets;
    size_t socket_count;
    size_t socket_capacity;
    event_callback_t* callbacks;
    void** user_data;
} epoll_context_t;

epoll_context_t* epoll_context_create(void);
void epoll_context_destroy(epoll_context_t* ctx);
chronos_error_t epoll_add_socket(epoll_context_t* ctx, socket_t* sock, event_callback_t callback, void* user_data);
chronos_error_t epoll_remove_socket(epoll_context_t* ctx, socket_t* sock);
chronos_error_t epoll_process_events(epoll_context_t* ctx, int timeout_ms);

