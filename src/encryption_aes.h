/**
 * 7z Encryption Support - Internal Header
 * 
 * Provides encryption/decryption functions for password-protected archives.
 */

#ifndef SEVENZIP_ENCRYPTION_H
#define SEVENZIP_ENCRYPTION_H

#include "../include/7z_ffi.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16
#define AES_NUM_IVMRK_WORDS ((1 + 1 + 15) * 4)

/**
 * Initialize AES encryption context from password
 * @param password Password string
 * @param key Output buffer for derived key (32 bytes)
 * @param iv Output buffer for IV (16 bytes)
 * @param aes_context Output AES context buffer
 * @return SEVENZIP_OK on success
 */
SevenZipErrorCode sevenzip_init_encryption(
    const char* password,
    uint8_t* key,
    uint8_t* iv,
    uint32_t* aes_context
);

/**
 * Initialize AES decryption context from password and salt
 * @param password Password string
 * @param salt Salt bytes used in key derivation
 * @param salt_len Length of salt
 * @param key Output buffer for derived key (32 bytes)
 * @param aes_context Output AES context buffer
 * @return SEVENZIP_OK on success
 */
SevenZipErrorCode sevenzip_init_decryption(
    const char* password,
    const uint8_t* salt,
    size_t salt_len,
    uint8_t* key,
    uint32_t* aes_context
);

/**
 * Encrypt data using AES-256-CBC
 * @param aes_context AES context from init_encryption
 * @param iv Initialization vector (16 bytes)
 * @param plaintext Input plaintext data
 * @param plaintext_len Length of plaintext
 * @param ciphertext Output buffer for ciphertext
 * @param ciphertext_len In: buffer size, Out: actual ciphertext length
 * @return SEVENZIP_OK on success
 */
SevenZipErrorCode sevenzip_encrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
);

/**
 * Decrypt data using AES-256-CBC
 * @param aes_context AES context from init_decryption
 * @param iv Initialization vector (16 bytes)
 * @param ciphertext Input ciphertext data
 * @param ciphertext_len Length of ciphertext
 * @param plaintext Output buffer for plaintext
 * @param plaintext_len In: buffer size, Out: actual plaintext length
 * @return SEVENZIP_OK on success, SEVENZIP_ERROR_EXTRACT if wrong password
 */
SevenZipErrorCode sevenzip_decrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* plaintext,
    size_t* plaintext_len
);

/**
 * Verify password is correct by decrypting test block
 * @param password Password to verify
 * @param encrypted_test_block Encrypted test data
 * @param test_block_len Length of test block
 * @param salt Salt used in encryption
 * @param salt_len Length of salt
 * @param iv Initialization vector
 * @return SEVENZIP_OK if password correct, error otherwise
 */
SevenZipErrorCode sevenzip_verify_password(
    const char* password,
    const uint8_t* encrypted_test_block,
    size_t test_block_len,
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* iv
);

#ifdef __cplusplus
}
#endif

#endif /* SEVENZIP_ENCRYPTION_H */
