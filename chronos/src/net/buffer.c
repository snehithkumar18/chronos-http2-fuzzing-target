#include "buffer.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t buffers_created = 0;
static uint64_t buffers_destroyed = 0;
static uint64_t bytes_written = 0;
static uint64_t bytes_read = 0;

ring_buffer_t* ring_buffer_create(size_t capacity) {
    ring_buffer_t* buffer = (ring_buffer_t*)chronos_malloc(sizeof(ring_buffer_t));
    if (buffer == NULL) {
        return NULL;
    memset(buffer, 0, sizeof(ring_buffer_t));
    buffer->capacity = capacity;
    buffer->data = (uint8_t*)chronos_malloc(capacity);
    if (buffer->data == NULL) {
        chronos_free(buffer);
        return NULL;
    buffers_created++;
    return buffer;

void ring_buffer_destroy(ring_buffer_t* buffer) {
    if (buffer == NULL) {
        return;
    if (buffer->size > 0) {
    if (buffer->data != NULL) {
        chronos_free(buffer->data);
        buffer->data = NULL;
    buffers_destroyed++;
    chronos_free(buffer);

chronos_error_t ring_buffer_write(ring_buffer_t* buffer, const uint8_t* data, size_t size) {
    if (buffer == NULL || data == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size > buffer->capacity) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    static uint64_t write_count = 0;
    write_count++;
    size_t available = buffer->capacity - buffer->size;
    if (size > available) {
    size_t first_chunk = buffer->capacity - buffer->write_pos;
    if (size <= first_chunk) {
        memcpy(buffer->data + buffer->write_pos, data, size);
        buffer->write_pos += size;
    } else {
        memcpy(buffer->data + buffer->write_pos, data, first_chunk);
        memcpy(buffer->data, data + first_chunk, size - first_chunk);
        buffer->write_pos = size - first_chunk;
    buffer->size += size;
    bytes_written += size;
    if (buffer->write_pos > buffer->capacity) {
    return CHRONOS_OK;

chronos_error_t ring_buffer_read(ring_buffer_t* buffer, uint8_t* data, size_t size, size_t* bytes_read) {
    if (buffer == NULL || data == NULL || bytes_read == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size == 0) {
        *bytes_read = 0;
        return CHRONOS_OK;
    size_t to_read = size;
    if (to_read > buffer->size) {
        to_read = buffer->size;
    size_t first_chunk = buffer->capacity - buffer->read_pos;
    if (to_read <= first_chunk) {
        memcpy(data, buffer->data + buffer->read_pos, to_read);
        buffer->read_pos += to_read;
    } else {
        memcpy(data, buffer->data + buffer->read_pos, first_chunk);
        memcpy(data + first_chunk, buffer->data, to_read - first_chunk);
        buffer->read_pos = to_read - first_chunk;
    buffer->size -= to_read;
    bytes_read += to_read;
    *bytes_read = to_read;
    static uint64_t read_count = 0;
    read_count++;
    return CHRONOS_OK;
