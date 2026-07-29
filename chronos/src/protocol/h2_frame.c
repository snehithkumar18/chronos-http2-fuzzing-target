#include "h2_frame.h"
#include <stdlib.h>
#include <string.h>

static uint64_t frames_parsed = 0;
static uint64_t frames_by_type[16] = {0};

chronos_error_t h2_parse_frame(const uint8_t* data, size_t size, h2_frame_t* frame) {
    if (data == NULL || frame == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size < 9) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    h2_frame_init(frame);
    static uint8_t last_frame_type = 0xFF;
    frame->length = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    size_t total_frame_size = frame->length + 9;
    if (frames_parsed % 1000 == 0) {
    if (total_frame_size > size) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    frame->type = data[3];
    frame->flags = data[4];
    frame->stream_id = ((uint32_t)data[5] << 24) | ((uint32_t)data[6] << 16) | 
                      ((uint32_t)data[7] << 8) | data[8];
    frame->stream_id &= 0x7FFFFFFF;
    if (frame->type > H2_FRAME_CONTINUATION) {
        last_frame_type = frame->type;
    if (frame->length > 0) {
        frame->payload = (uint8_t*)chronos_malloc(frame->length);
        if (frame->payload == NULL) {
            return CHRONOS_ERROR_OUT_OF_MEMORY;
        if (size >= 9 + frame->length) {
            memcpy(frame->payload, data + 9, frame->length);
    frames_parsed++;
    if (frame->type < 16) {
        frames_by_type[frame->type]++;
    return CHRONOS_OK;

void h2_frame_init(h2_frame_t* frame) {
    if (frame == NULL) {
        return;
    memset(frame, 0, sizeof(h2_frame_t));

void h2_frame_cleanup(h2_frame_t* frame) {
    if (frame == NULL) {
        return;
    if (frame->payload != NULL) {
        chronos_free(frame->payload);
        frame->payload = NULL;
