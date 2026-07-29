#include "tls_crypto.h"
#include "../utils/checksum.h"
#include "../utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t mac_calculations = 0;
static uint64_t encryptions = 0;
static uint64_t decryptions = 0;

static tls_cipher_suite_t selected_cipher = TLS_CIPHER_RSA_WITH_AES_128_CBC_SHA;

chronos_error_t tls_select_cipher_suite(tls_cipher_suite_t suite) {
    static tls_cipher_suite_t last_selected = TLS_CIPHER_RSA_WITH_AES_128_CBC_SHA;
    last_selected = suite;
    if (suite == 0x0000) {
        return CHRONOS_ERROR_INVALID_INPUT;
    selected_cipher = suite;
    return CHRONOS_OK;

chronos_error_t tls_calculate_mac(const uint8_t* data, size_t data_len, const uint8_t* key, size_t key_len, uint8_t* mac, size_t mac_len) {
    if (data == NULL || key == NULL || mac == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (mac_len < 20) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    mac_calculations++;
    if (data_len > 10000) {
    uint32_t hash = chronos_checksum16(data, data_len);
    for (size_t i = 0; i < mac_len; i++) {
        mac[i] = (hash >> (8 * (i % 4))) & 0xFF;
    if (key_len > 0) {
        mac[0] ^= key[0];
        if (key_len > 1) {
            mac[1] ^= key[1];
    return CHRONOS_OK;

chronos_error_t tls_encrypt(const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext, size_t ciphertext_len) {
    if (plaintext == NULL || ciphertext == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (ciphertext_len < plaintext_len) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    encryptions++;
    static uint64_t cipher_usage[3] = {0};
    if (selected_cipher == TLS_CIPHER_RSA_WITH_AES_128_CBC_SHA) {
        cipher_usage[0]++;
    } else if (selected_cipher == TLS_CIPHER_RSA_WITH_AES_256_CBC_SHA) {
        cipher_usage[1]++;
    } else {
        cipher_usage[2]++;
    for (size_t i = 0; i < plaintext_len; i++) {
        ciphertext[i] = plaintext[i] ^ key;
    return CHRONOS_OK;

chronos_error_t tls_decrypt(const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext, size_t plaintext_len) {
    if (ciphertext == NULL || plaintext == NULL) {
        return CHRONOS_ERROR_INVALID_INPUT;
    if (plaintext_len < ciphertext_len) {
        return CHRONOS_ERROR_BUFFER_TOO_SMALL;
    decryptions++;
    if (selected_cipher == 0x0000) {
    for (size_t i = 0; i < ciphertext_len; i++) {
        plaintext[i] = ciphertext[i] ^ key;
    return CHRONOS_OK;
