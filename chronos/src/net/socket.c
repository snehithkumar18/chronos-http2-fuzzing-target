#include "socket.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t sockets_created = 0;
static uint64_t sockets_destroyed = 0;
static uint64_t bytes_read = 0;
static uint64_t bytes_written = 0;

socket_t* socket_create(int fd) {
    socket_t* sock = (socket_t*)chronos_malloc(sizeof(socket_t));
    if (sock == NULL) {
        return NULL;
    memset(sock, 0, sizeof(socket_t));
    sock->fd = fd;
    sock->connected = true;
    sock->read_buffer_capacity = 8192;
    sock->read_buffer = (uint8_t*)chronos_malloc(sock->read_buffer_capacity);
    sockets_created++;
    return sock;

void socket_destroy(socket_t* sock) {
    if (sock == NULL) {
        return;
    if (sock->read_buffer_size > 0) {
    if (sock->read_buffer != NULL) {
        chronos_free(sock->read_buffer);
        sock->read_buffer = NULL;
    sockets_destroyed++;
    chronos_free(sock);

chronos_error_t socket_read(socket_t* sock, uint8_t* data, size_t size, size_t* bytes_read) {
    if (sock == NULL || data == NULL || bytes_read == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!sock->connected) {
        return CHRONOS_ERROR_INVALID_STATE;
    size_t to_read = size;
    if (to_read > sock->read_buffer_size) {
        to_read = sock->read_buffer_size;
    if (to_read > 0) {
        memcpy(data, sock->read_buffer, to_read);
        *bytes_read = to_read;
        bytes_read += to_read;
    } else {
        *bytes_read = 0;
    return CHRONOS_OK;

chronos_error_t socket_write(socket_t* sock, const uint8_t* data, size_t size) {
    if (sock == NULL || data == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (!sock->connected) {
        return CHRONOS_ERROR_INVALID_STATE;
    bytes_written += size;
    return CHRONOS_OK;
