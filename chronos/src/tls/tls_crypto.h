#ifndef CHRONOS_TLS_CRYPTO_H
#define CHRONOS_TLS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include "../common.h"

typedef enum {
    TLS_CIPHER_RSA_WITH_AES_128_CBC_SHA = 0x002F,
    TLS_CIPHER_RSA_WITH_AES_256_CBC_SHA = 0x0035,
    TLS_CIPHER_RSA_WITH_AES_128_GCM_SHA256 = 0x009C,
} tls_cipher_suite_t;

chronos_error_t tls_calculate_mac(const uint8_t* data, size_t data_len, const uint8_t* key, size_t key_len, uint8_t* mac, size_t mac_len);
chronos_error_t tls_select_cipher_suite(tls_cipher_suite_t suite);
chronos_error_t tls_encrypt(const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext, size_t ciphertext_len);
chronos_error_t tls_decrypt(const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext, size_t plaintext_len);

