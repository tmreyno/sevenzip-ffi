/**
 * 7z Encryption Support with AES-256
 * 
 * Implements password-based encryption using AES-256 with SHA-256 key derivation.
 * Compatible with 7-Zip password protection format.
 */

#include "../include/7z_ffi.h"
#include "7z.h"
#include "Aes.h"
#include "Sha256.h"
#include "7zCrc.h"
#include <string.h>
#include <stdlib.h>

#define AES_KEY_SIZE 32  // 256 bits
#ifndef SHA256_DIGEST_SIZE
#define SHA256_DIGEST_SIZE 32
#endif
#define AES_BLOCK_SIZE 16
#define PBKDF2_ITERATIONS 262144  // 256K iterations (7-Zip default)

/**
 * PBKDF2-SHA256 key derivation (simplified version)
 * In production, use a full PBKDF2 implementation
 */
static void derive_key_from_password(
    const char* password,
    const uint8_t* salt,
    size_t salt_len,
    uint32_t iterations,
    uint8_t* key,
    size_t key_len
) {
    CSha256 sha;
    uint8_t hash[SHA256_DIGEST_SIZE];
    size_t password_len = strlen(password);
    
    // Initialize SHA256
    Sha256_Init(&sha);
    
    // First iteration: hash(password + salt)
    Sha256_Update(&sha, (const Byte*)password, password_len);
    Sha256_Update(&sha, salt, salt_len);
    Sha256_Final(&sha, hash);
    
    // Additional iterations: hash(previous_hash)
    for (uint32_t i = 1; i < iterations; i++) {
        Sha256_Init(&sha);
        Sha256_Update(&sha, hash, SHA256_DIGEST_SIZE);
        Sha256_Final(&sha, hash);
    }
    
    // Copy to output key
    memcpy(key, hash, (key_len < SHA256_DIGEST_SIZE) ? key_len : SHA256_DIGEST_SIZE);
}

/**
 * Initialize AES encryption context
 */
SevenZipErrorCode sevenzip_init_encryption(
    const char* password,
    uint8_t* key,
    uint8_t* iv,
    uint32_t* aes_context
) {
    if (!password || !key || !iv || !aes_context) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Generate random salt (in production, use crypto-secure random)
    uint8_t salt[16];
    for (int i = 0; i < 16; i++) {
        salt[i] = (uint8_t)(rand() & 0xFF);
    }
    
    // Derive encryption key from password
    derive_key_from_password(password, salt, 16, PBKDF2_ITERATIONS, key, AES_KEY_SIZE);
    
    // Initialize IV (in production, use crypto-secure random)
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        iv[i] = (uint8_t)(rand() & 0xFF);
    }
    
    // Initialize AES tables
    AesGenTables();
    
    // Set up AES encryption key
    Aes_SetKey_Enc(aes_context, key, AES_KEY_SIZE);
    
    return SEVENZIP_OK;
}

/**
 * Initialize AES decryption context
 */
SevenZipErrorCode sevenzip_init_decryption(
    const char* password,
    const uint8_t* salt,
    size_t salt_len,
    uint8_t* key,
    uint32_t* aes_context
) {
    if (!password || !salt || !key || !aes_context) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Derive decryption key from password
    derive_key_from_password(password, salt, salt_len, PBKDF2_ITERATIONS, key, AES_KEY_SIZE);
    
    // Initialize AES tables
    AesGenTables();
    
    // Set up AES decryption key
    Aes_SetKey_Dec(aes_context, key, AES_KEY_SIZE);
    
    return SEVENZIP_OK;
}

/**
 * Encrypt data using AES-256-CBC
 */
SevenZipErrorCode sevenzip_encrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
) {
    if (!aes_context || !iv || !plaintext || !ciphertext || !ciphertext_len) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Calculate padded length (PKCS#7 padding)
    size_t padded_len = ((plaintext_len / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;
    if (*ciphertext_len < padded_len) {
        *ciphertext_len = padded_len;
        return SEVENZIP_ERROR_MEMORY;
    }
    
    // Allocate aligned buffer for padded data
    uint8_t* padded_data = (uint8_t*)aligned_alloc(16, padded_len);
    if (!padded_data) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    memcpy(padded_data, plaintext, plaintext_len);
    uint8_t padding_byte = (uint8_t)(padded_len - plaintext_len);
    for (size_t i = plaintext_len; i < padded_len; i++) {
        padded_data[i] = padding_byte;
    }
    
    // Allocate aligned buffer for AES context with IV
    uint32_t* ivAes = (uint32_t*)aligned_alloc(16, AES_NUM_IVMRK_WORDS * sizeof(uint32_t));
    if (!ivAes) {
        free(padded_data);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    // Initialize CBC mode with IV
    AesCbc_Init(ivAes, iv);
    
    // Copy AES key schedule to ivAes
    memcpy(ivAes + 4, aes_context, (AES_NUM_IVMRK_WORDS - 4) * sizeof(uint32_t));
    
    // Copy padded data to ciphertext (must be aligned for AES)
    memcpy(ciphertext, padded_data, padded_len);
    
    // Encrypt data in-place using AES-CBC
    size_t num_blocks = padded_len / AES_BLOCK_SIZE;
    AesCbc_Encode(ivAes, ciphertext, num_blocks);
    
    *ciphertext_len = padded_len;
    
    free(padded_data);
    free(ivAes);
    
    return SEVENZIP_OK;
}

/**
 * Decrypt data using AES-256-CBC
 */
SevenZipErrorCode sevenzip_decrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* plaintext,
    size_t* plaintext_len
) {
    if (!aes_context || !iv || !ciphertext || !plaintext || !plaintext_len) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Ciphertext must be multiple of block size
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    if (*plaintext_len < ciphertext_len) {
        *plaintext_len = ciphertext_len;
        return SEVENZIP_ERROR_MEMORY;
    }
    
    // Allocate aligned buffer for AES context with IV
    uint32_t* ivAes = (uint32_t*)aligned_alloc(16, AES_NUM_IVMRK_WORDS * sizeof(uint32_t));
    if (!ivAes) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    // Initialize CBC mode with IV
    AesCbc_Init(ivAes, iv);
    
    // Copy AES key schedule to ivAes
    memcpy(ivAes + 4, aes_context, (AES_NUM_IVMRK_WORDS - 4) * sizeof(uint32_t));
    
    // Copy ciphertext to plaintext buffer for in-place decryption
    memcpy(plaintext, ciphertext, ciphertext_len);
    
    // Decrypt data in-place using AES-CBC
    size_t num_blocks = ciphertext_len / AES_BLOCK_SIZE;
    g_AesCbc_Decode(ivAes, plaintext, num_blocks);
    
    // Remove PKCS#7 padding
    uint8_t padding_byte = plaintext[ciphertext_len - 1];
    if (padding_byte > 0 && padding_byte <= AES_BLOCK_SIZE) {
        // Verify padding
        for (size_t i = ciphertext_len - padding_byte; i < ciphertext_len; i++) {
            if (plaintext[i] != padding_byte) {
                free(ivAes);
                return SEVENZIP_ERROR_EXTRACT; // Invalid padding = wrong password
            }
        }
        *plaintext_len = ciphertext_len - padding_byte;
    } else {
        *plaintext_len = ciphertext_len;
    }
    
    free(ivAes);
    
    return SEVENZIP_OK;
}

/**
 * Verify password by attempting to decrypt a test block
 */
SevenZipErrorCode sevenzip_verify_password(
    const char* password,
    const uint8_t* encrypted_test_block,
    size_t test_block_len,
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* iv
) {
    if (!password || !encrypted_test_block || !salt || !iv) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    uint8_t key[AES_KEY_SIZE];
    uint32_t aes_context[AES_NUM_IVMRK_WORDS];
    
    // Initialize decryption
    SevenZipErrorCode result = sevenzip_init_decryption(
        password, salt, salt_len, key, aes_context
    );
    
    if (result != SEVENZIP_OK) {
        return result;
    }
    
    // Try to decrypt test block
    uint8_t* decrypted = (uint8_t*)malloc(test_block_len);
    if (!decrypted) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    size_t decrypted_len = test_block_len;
    result = sevenzip_decrypt_data(
        aes_context, iv, encrypted_test_block, test_block_len,
        decrypted, &decrypted_len
    );
    
    free(decrypted);
    
    // If decryption succeeds with valid padding, password is correct
    return result;
}
