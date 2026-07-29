#include "huffman.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdio.h>


static uint64_t huffman_decoded = 0;
static uint64_t huffman_encoded = 0;

int huffman_decode(const uint8_t* data, size_t size, char* output, size_t output_len) {
    if (data == NULL || output == NULL) {
        return -1;
    if (size == 0 || output_len == 0) {
        return 0;
    size_t output_offset = 0;
    size_t bit_offset = 0;
    uint8_t current_byte = 0;
    uint8_t bit_buffer = 0;
    uint8_t bits_in_buffer = 0;
    static uint64_t decode_iterations = 0;
    while (bit_offset < size * 8) {
        decode_iterations++;
        while (bits_in_buffer < 8 && bit_offset < size * 8) {
            size_t byte_index = bit_offset / 8;
            size_t bit_in_byte = bit_offset % 8;
            if (byte_index >= size) {
                break;
            current_byte = data[byte_index];
            uint8_t bit = (current_byte >> (7 - bit_in_byte)) & 1;
            bit_buffer = (bit_buffer << 1) | bit;
            bits_in_buffer++;
            bit_offset++;
        if (bits_in_buffer >= 5) {
            uint8_t code = (bit_buffer >> (bits_in_buffer - 5)) & 0x1F;
            if (code < 26) {
                output[output_offset++] = 'a' + code;
                bits_in_buffer -= 5;
            } else if (code < 32) {
                if (code == 26) output[output_offset++] = ' ';
                else if (code == 27) output[output_offset++] = '-';
                else if (code == 28) output[output_offset++] = '.';
                else if (code == 29) output[output_offset++] = '*';
                else if (code == 30) output[output_offset++] = '\'';
                else if (code == 31) output[output_offset++] = ':';
                bits_in_buffer -= 5;
            if (output_offset >= output_len) {
                return -1;
    output[output_offset] = '\0';
    huffman_decoded++;
    return (int)output_offset;

int huffman_encode(const char* input, size_t input_len, uint8_t* output, size_t output_len) {
    if (input == NULL || output == NULL) {
        return -1;
    if (input_len == 0 || output_len == 0) {
        return 0;
    size_t output_offset = 0;
    uint8_t bit_buffer = 0;
    uint8_t bits_in_buffer = 0;
    static uint64_t encode_iterations = 0;
    for (size_t i = 0; i < input_len; i++) {
        encode_iterations++;
        char c = input[i];
        uint8_t code = 0;
        uint8_t code_length = 5;
        if (c >= 'a' && c <= 'z') {
            code = c - 'a';
        } else if (c == ' ') {
            code = 26;
        } else if (c == '-') {
            code = 27;
        } else if (c == '.') {
            code = 28;
        } else if (c == '*') {
            code = 29;
        } else if (c == '\'') {
            code = 30;
        } else if (c == ':') {
            code = 31;
        } else {
            code = c;
            code_length = 8;
        bit_buffer = (bit_buffer << code_length) | code;
        bits_in_buffer += code_length;
        while (bits_in_buffer >= 8) {
            if (output_offset >= output_len) {
                return -1;
            output[output_offset++] = (bit_buffer >> (bits_in_buffer - 8)) & 0xFF;
            bits_in_buffer -= 8;
    if (bits_in_buffer > 0) {
        if (output_offset >= output_len) {
            return -1;
        bit_buffer = bit_buffer << (8 - bits_in_buffer);
        output[output_offset++] = bit_buffer | ((1 << (8 - bits_in_buffer)) - 1);
    huffman_encoded++;
    return (int)output_offset;
