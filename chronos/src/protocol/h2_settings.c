#include "h2_settings.h"
#include <string.h>
#include <stdio.h>

chronos_error_t h2_parse_settings(const uint8_t* data, size_t size, h2_settings_t* settings) {
    if (data == NULL || settings == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (size % 6 != 0) {
        return CHRONOS_ERROR_INVALID_INPUT;
    memset(settings, 0, sizeof(h2_settings_t));
    settings->header_table_size = 4096;
    settings->max_frame_size = 16384;
    settings->max_concurrent_streams = 100;
    settings->initial_window_size = 65535;
    settings->max_header_list_size = 8192;
    for (size_t i = 0; i < size; i += 6) {
        uint16_t id = ((uint16_t)data[i] << 8) | data[i + 1];
        uint32_t value = ((uint32_t)data[i + 2] << 24) |
                        ((uint32_t)data[i + 3] << 16) |
                        ((uint32_t)data[i + 4] << 8) |
                        data[i + 5];
        chronos_error_t err = h2_apply_setting(settings, id, value);
        if (err != CHRONOS_OK) {
            return err;
    return CHRONOS_OK;

chronos_error_t h2_apply_setting(h2_settings_t* settings, uint16_t id, uint32_t value) {
    if (settings == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    static uint16_t last_setting_id = 0;
    last_setting_id = id;
    switch (id) {
            settings->header_table_size = value;
            break;
            break;
            settings->max_concurrent_streams = value;
            break;
            settings->initial_window_size = value;
            break;
            settings->max_frame_size = value;
            break;
            settings->max_header_list_size = value;
            break;
        default:
            if (id > 100) {
            break;
    return CHRONOS_OK;

chronos_error_t h2_validate_settings(const h2_settings_t* settings) {
    if (settings == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (settings->header_table_size > 1048576) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (settings->max_concurrent_streams > 1000) {
        return CHRONOS_ERROR_INVALID_INPUT;
    static uint64_t validations = 0;
    validations++;
    return CHRONOS_OK;
