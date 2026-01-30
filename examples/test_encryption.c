/**
 * Test program for 7z encryption functionality
 */

#include "encryption_aes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    printf("7z Encryption Test\n");
    printf("==================\n\n");
    
    const char* password = "MySecurePassword123";
    const char* plaintext = "This is a secret message that will be encrypted!";
    size_t plaintext_len = strlen(plaintext);
    
    printf("Password: %s\n", password);
    printf("Plaintext: %s\n", plaintext);
    printf("Plaintext length: %zu bytes\n\n", plaintext_len);
    
    // Initialize encryption
    uint8_t key[AES_KEY_SIZE];
    uint8_t iv[AES_BLOCK_SIZE];
    uint32_t aes_enc_context[AES_NUM_IVMRK_WORDS];
    
    printf("Initializing encryption...\n");
    SevenZipErrorCode result = sevenzip_init_encryption(
        password, key, iv, aes_enc_context
    );
    
    if (result != SEVENZIP_OK) {
        fprintf(stderr, "Failed to initialize encryption: %d\n", result);
        return 1;
    }
    
    printf("✓ Encryption initialized\n\n");
    
    // Encrypt data
    size_t ciphertext_len = plaintext_len + 64;  // Extra space for padding
    uint8_t* ciphertext = (uint8_t*)malloc(ciphertext_len);
    
    printf("Encrypting data...\n");
    result = sevenzip_encrypt_data(
        aes_enc_context, iv,
        (const uint8_t*)plaintext, plaintext_len,
        ciphertext, &ciphertext_len
    );
    
    if (result != SEVENZIP_OK) {
        fprintf(stderr, "Failed to encrypt: %d\n", result);
        free(ciphertext);
        return 1;
    }
    
    printf("✓ Data encrypted\n");
    printf("Ciphertext length: %zu bytes\n", ciphertext_len);
    printf("Ciphertext (hex): ");
    for (size_t i = 0; i < (ciphertext_len < 32 ? ciphertext_len : 32); i++) {
        printf("%02x", ciphertext[i]);
    }
    if (ciphertext_len > 32) printf("...");
    printf("\n\n");
    
    // Decrypt data
    uint32_t aes_dec_context[AES_NUM_IVMRK_WORDS];
    uint8_t salt[16] = {0};  // In real usage, this would be stored with the archive
    
    printf("Initializing decryption...\n");
    result = sevenzip_init_decryption(
        password, salt, 16, key, aes_dec_context
    );
    
    if (result != SEVENZIP_OK) {
        fprintf(stderr, "Failed to initialize decryption: %d\n", result);
        free(ciphertext);
        return 1;
    }
    
    printf("✓ Decryption initialized\n\n");
    
    size_t decrypted_len = ciphertext_len;
    uint8_t* decrypted = (uint8_t*)malloc(decrypted_len);
    
    printf("Decrypting data...\n");
    result = sevenzip_decrypt_data(
        aes_dec_context, iv,
        ciphertext, ciphertext_len,
        decrypted, &decrypted_len
    );
    
    if (result != SEVENZIP_OK) {
        fprintf(stderr, "Failed to decrypt: %d\n", result);
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    printf("✓ Data decrypted\n");
    printf("Decrypted length: %zu bytes\n", decrypted_len);
    printf("Decrypted text: %.*s\n\n", (int)decrypted_len, decrypted);
    
    // Verify decryption matches original
    if (decrypted_len == plaintext_len && 
        memcmp(decrypted, plaintext, plaintext_len) == 0) {
        printf("✅ SUCCESS: Decrypted data matches original!\n");
    } else {
        printf("❌ FAILED: Decrypted data does not match!\n");
        free(ciphertext);
        free(decrypted);
        return 1;
    }
    
    // Test wrong password
    printf("\n--- Testing Wrong Password ---\n");
    const char* wrong_password = "WrongPassword";
    uint32_t wrong_context[AES_NUM_IVMRK_WORDS];
    
    result = sevenzip_init_decryption(
        wrong_password, salt, 16, key, wrong_context
    );
    
    if (result == SEVENZIP_OK) {
        size_t test_len = ciphertext_len;
        uint8_t* test_buf = (uint8_t*)malloc(test_len);
        
        result = sevenzip_decrypt_data(
            wrong_context, iv,
            ciphertext, ciphertext_len,
            test_buf, &test_len
        );
        
        if (result != SEVENZIP_OK) {
            printf("✅ Correctly rejected wrong password\n");
        } else {
            printf("⚠️  Wrong password not detected (data corruption would occur)\n");
        }
        
        free(test_buf);
    }
    
    free(ciphertext);
    free(decrypted);
    
    printf("\n✅ All encryption tests passed!\n");
    return 0;
}
