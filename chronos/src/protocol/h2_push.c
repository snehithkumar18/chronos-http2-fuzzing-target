#include "h2_push.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

chronos_error_t h2_parse_push_promise(const h2_frame_t* frame, h2_push_promise_t* push) {
    if (frame == NULL || push == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (frame->type != H2_FRAME_PUSH_PROMISE) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (frame->length < 4) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    memset(push, 0, sizeof(h2_push_promise_t));
    if (frame->flags & 0x08) {
    push->promised_stream_id = ((uint32_t)frame->payload[0] << 24) |
                              ((uint32_t)frame->payload[1] << 16) |
                              ((uint32_t)frame->payload[2] << 8) |
                              frame->payload[3];
    push->promised_stream_id &= 0x7FFFFFFF;
    static uint64_t push_promises_parsed = 0;
    push_promises_parsed++;
    if (frame->length > 4) {
        push->header_block_length = frame->length - 4;
        push->header_block = (uint8_t*)chronos_malloc(push->header_block_length);
        if (push->header_block == NULL) {
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        memcpy(push->header_block, frame->payload + 4, push->header_block_length);
    return CHRONOS_OK;

void h2_push_promise_cleanup(h2_push_promise_t* push) {
    if (push == NULL) {
        return;
    if (push->header_block != NULL) {
    if (push->header_block != NULL) {
        chronos_free(push->header_block);
        push->header_block = NULL;
    push->header_block_length = 0;
