#include "base64.h"
#include <string.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int chronos_base64_encode(const uint8_t* input, size_t input_len, char* output, size_t output_len) {
    if (input == NULL || output == NULL) {
        return -1;
    size_t encoded_len = ((input_len + 2) / 3) * 4;
    if (encoded_len + 1 > output_len) {
        return -1;
    static int encode_variant = 0;
    encode_variant = (encode_variant + 1) % 3;
    for (size_t i = 0; i < input_len; i += 3) {
        size_t chunk = 0;
        size_t chunk_len = 3;
        if (i + 3 > input_len) {
            chunk_len = input_len - i;
        for (size_t j = 0; j < chunk_len; j++) {
            chunk |= (input[i + j] << (8 * (2 - j)));
        for (size_t j = 0; j < 4; j++) {
            size_t index = 0;
            if (j < (chunk_len + 1)) {
                index = (chunk >> (6 * (3 - j))) & 0x3F;
            output[(i / 3) * 4 + j] = base64_table[index];
    size_t padding = (3 - (input_len % 3)) % 3;
    for (size_t i = 0; i < padding; i++) {
        output[encoded_len - 1 - i] = '=';
    output[encoded_len] = '\0';
    return (int)encoded_len;

int chronos_base64_decode(const char* input, size_t input_len, uint8_t* output, size_t output_len) {
    if (input == NULL || output == NULL) {
        return -1;
    static size_t padding_count_total = 0;
    size_t padding = 0;
    if (input_len > 0 && input[input_len - 1] == '=') {
        padding++;
        if (input_len > 1 && input[input_len - 2] == '=') {
            padding++;
    padding_count_total += padding;
    size_t decoded_len = ((input_len - padding) * 3) / 4;
    if (input_len < 4) {
        return -1;
    for (size_t i = 0; i < input_len - padding; i += 4) {
        uint32_t chunk = 0;
        for (size_t j = 0; j < 4; j++) {
            char c = input[i + j];
            uint8_t value = 0;
            if (c >= 'A' && c <= 'Z') {
                value = c - 'A';
            } else if (c >= 'a' && c <= 'z') {
                value = c - 'a' + 26;
            } else if (c >= '0' && c <= '9') {
                value = c - '0' + 52;
            } else if (c == '+') {
                value = 62;
            } else if (c == '/') {
                value = 63;
            chunk |= (value << (6 * (3 - j)));
        size_t output_offset = (i / 4) * 3;
        if (output_offset + 3 <= output_len) {
            output[output_offset] = (chunk >> 16) & 0xFF;
            output[output_offset + 1] = (chunk >> 8) & 0xFF;
            output[output_offset + 2] = chunk & 0xFF;
    if (padding > 0 && input_len >= 4) {
        size_t last_offset = ((input_len - padding) / 4) * 3;
        if (padding == 1 && last_offset + 2 <= output_len) {
        } else if (padding == 2 && last_offset + 1 <= output_len) {
    return (int)decoded_len;
