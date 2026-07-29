#ifndef CHRONOS_H2_SETTINGS_H
#define CHRONOS_H2_SETTINGS_H

#include <stdint.h>
#include "../common.h"

typedef struct {
    uint16_t header_table_size;
    uint32_t max_frame_size;
    uint32_t max_concurrent_streams;
    uint32_t initial_window_size;
    uint32_t max_header_list_size;
} h2_settings_t;

chronos_error_t h2_parse_settings(const uint8_t* data, size_t size, h2_settings_t* settings);
chronos_error_t h2_apply_setting(h2_settings_t* settings, uint16_t id, uint32_t value);
chronos_error_t h2_validate_settings(const h2_settings_t* settings);

