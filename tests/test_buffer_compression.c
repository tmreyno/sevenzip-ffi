/**
 * Comprehensive Unit Tests for Buffer Compression API
 * Tests LZMA compression/decompression, XZ format, version info
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test results structure
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestResults;

static TestResults results = {0, 0, 0};

#define TEST_START(name) \
    do { \
        results.total_tests++; \
        printf("  Testing %s... ", name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() \
    do { \
        results.passed_tests++; \
        printf("✓ PASS\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        results.failed_tests++; \
        printf("✗ FAIL: %s\n", msg); \
    } while(0)

/**
 * Test basic LZMA compression and decompression
 */
void test_basic_compression(void) {
    TEST_START("Basic LZMA compression/decompression");
    
    // Original data
    const char* original = "Hello, World! This is a test of LZMA compression.";
    size_t original_size = strlen(original) + 1;  // Include null terminator
    
    // Allocate compression buffer
    size_t compressed_size = sevenzip_compress_bound(original_size);
    if (compressed_size == 0) {
        TEST_FAIL("compress_bound returned 0");
        return;
    }
    
    unsigned char* compressed = malloc(compressed_size);
    if (!compressed) {
        TEST_FAIL("Failed to allocate compression buffer");
        return;
    }
    
    // Compress
    size_t actual_compressed_size = compressed_size;
    int result = sevenzip_compress_buffer(
        (const unsigned char*)original,
        original_size,
        compressed,
        &actual_compressed_size,
        5  // Compression level
    );
    
    if (result != 0) {
        TEST_FAIL("Compression failed");
        free(compressed);
        return;
    }
    
    if (actual_compressed_size >= original_size) {
        printf("Warning: Compressed size (%zu) >= original (%zu)\n", 
               actual_compressed_size, original_size);
    }
    
    // Allocate decompression buffer
    unsigned char* decompressed = malloc(original_size);
    if (!decompressed) {
        TEST_FAIL("Failed to allocate decompression buffer");
        free(compressed);
        return;
    }
    
    // Decompress
    size_t decompressed_size = original_size;
    result = sevenzip_decompress_buffer(
        compressed,
        actual_compressed_size,
        decompressed,
        &decompressed_size
    );
    
    if (result != 0) {
        TEST_FAIL("Decompression failed");
        free(compressed);
        free(decompressed);
        return;
    }
    
    if (decompressed_size != original_size) {
        TEST_FAIL("Decompressed size mismatch");
        free(compressed);
        free(decompressed);
        return;
    }
    
    // Verify content
    if (memcmp(decompressed, original, original_size) != 0) {
        TEST_FAIL("Decompressed data doesn't match original");
        free(compressed);
        free(decompressed);
        return;
    }
    
    TEST_PASS();
    
    free(compressed);
    free(decompressed);
}

/**
 * Test compression with different levels (0-9)
 */
void test_compression_levels(void) {
    TEST_START("Compression levels (0-9)");
    
    const char* data = "The quick brown fox jumps over the lazy dog. "
                       "The quick brown fox jumps over the lazy dog. "
                       "The quick brown fox jumps over the lazy dog.";
    size_t data_size = strlen(data) + 1;
    
    for (int level = 0; level <= 9; level++) {
        size_t compressed_size = sevenzip_compress_bound(data_size);
        unsigned char* compressed = malloc(compressed_size);
        
        size_t actual_size = compressed_size;
        int result = sevenzip_compress_buffer(
            (const unsigned char*)data,
            data_size,
            compressed,
            &actual_size,
            level
        );
        
        if (result != 0) {
            printf("\n    Level %d failed", level);
            TEST_FAIL("Compression failed at various levels");
            free(compressed);
            return;
        }
        
        free(compressed);
    }
    
    TEST_PASS();
}

/**
 * Test XZ format compression and decompression
 */
void test_xz_format(void) {
    TEST_START("XZ format compression/decompression");
    
    const char* original = "Testing XZ (LZMA2) compression format with CRC64 checksums.";
    size_t original_size = strlen(original) + 1;
    
    // Compress to XZ format
    size_t compressed_size = sevenzip_compress_bound(original_size) + 256;  // XZ header overhead
    unsigned char* compressed = malloc(compressed_size);
    
    size_t actual_compressed_size = compressed_size;
    int result = sevenzip_compress_xz(
        (const unsigned char*)original,
        original_size,
        compressed,
        &actual_compressed_size,
        6  // Compression level
    );
    
    if (result != 0) {
        TEST_FAIL("XZ compression failed");
        free(compressed);
        return;
    }
    
    // Verify XZ magic header (0xFD, '7', 'z', 'X', 'Z', 0x00)
    if (compressed[0] != 0xFD || compressed[1] != '7' || 
        compressed[2] != 'z' || compressed[3] != 'X' || 
        compressed[4] != 'Z' || compressed[5] != 0x00) {
        TEST_FAIL("Invalid XZ header");
        free(compressed);
        return;
    }
    
    // Decompress
    unsigned char* decompressed = malloc(original_size);
    size_t decompressed_size = original_size;
    
    result = sevenzip_decompress_xz(
        compressed,
        actual_compressed_size,
        decompressed,
        &decompressed_size
    );
    
    if (result != 0) {
        TEST_FAIL("XZ decompression failed");
        free(compressed);
        free(decompressed);
        return;
    }
    
    if (decompressed_size != original_size) {
        TEST_FAIL("XZ decompressed size mismatch");
        free(compressed);
        free(decompressed);
        return;
    }
    
    if (memcmp(decompressed, original, original_size) != 0) {
        TEST_FAIL("XZ decompressed data mismatch");
        free(compressed);
        free(decompressed);
        return;
    }
    
    TEST_PASS();
    
    free(compressed);
    free(decompressed);
}

/**
 * Test large data compression
 */
void test_large_data(void) {
    TEST_START("Large data compression (1MB)");
    
    // Create 1MB of repeating pattern
    size_t data_size = 1024 * 1024;
    unsigned char* data = malloc(data_size);
    if (!data) {
        TEST_FAIL("Failed to allocate test data");
        return;
    }
    
    // Fill with pattern
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (unsigned char)(i % 256);
    }
    
    // Compress
    size_t compressed_size = sevenzip_compress_bound(data_size);
    unsigned char* compressed = malloc(compressed_size);
    if (!compressed) {
        TEST_FAIL("Failed to allocate compression buffer");
        free(data);
        return;
    }
    
    size_t actual_compressed_size = compressed_size;
    int result = sevenzip_compress_buffer(
        data,
        data_size,
        compressed,
        &actual_compressed_size,
        5
    );
    
    if (result != 0) {
        TEST_FAIL("Large data compression failed");
        free(data);
        free(compressed);
        return;
    }
    
    // Should achieve good compression on pattern data
    double ratio = (double)actual_compressed_size / data_size;
    if (ratio > 0.5) {  // Should compress to less than 50%
        printf("\n    Warning: Poor compression ratio: %.2f%%", ratio * 100);
    }
    
    // Decompress
    unsigned char* decompressed = malloc(data_size);
    if (!decompressed) {
        TEST_FAIL("Failed to allocate decompression buffer");
        free(data);
        free(compressed);
        return;
    }
    
    size_t decompressed_size = data_size;
    result = sevenzip_decompress_buffer(
        compressed,
        actual_compressed_size,
        decompressed,
        &decompressed_size
    );
    
    if (result != 0) {
        TEST_FAIL("Large data decompression failed");
        free(data);
        free(compressed);
        free(decompressed);
        return;
    }
    
    if (memcmp(decompressed, data, data_size) != 0) {
        TEST_FAIL("Large data verification failed");
        free(data);
        free(compressed);
        free(decompressed);
        return;
    }
    
    TEST_PASS();
    
    free(data);
    free(compressed);
    free(decompressed);
}

/**
 * Test binary data compression
 */
void test_binary_data(void) {
    TEST_START("Binary data compression");
    
    // Create binary test data with various byte values
    unsigned char binary_data[256];
    for (int i = 0; i < 256; i++) {
        binary_data[i] = (unsigned char)i;
    }
    
    size_t compressed_size = sevenzip_compress_bound(sizeof(binary_data));
    unsigned char* compressed = malloc(compressed_size);
    
    size_t actual_size = compressed_size;
    int result = sevenzip_compress_buffer(
        binary_data,
        sizeof(binary_data),
        compressed,
        &actual_size,
        9  // Maximum compression
    );
    
    if (result != 0) {
        TEST_FAIL("Binary data compression failed");
        free(compressed);
        return;
    }
    
    // Decompress
    unsigned char* decompressed = malloc(sizeof(binary_data));
    size_t decompressed_size = sizeof(binary_data);
    
    result = sevenzip_decompress_buffer(
        compressed,
        actual_size,
        decompressed,
        &decompressed_size
    );
    
    if (result != 0) {
        TEST_FAIL("Binary data decompression failed");
        free(compressed);
        free(decompressed);
        return;
    }
    
    if (memcmp(decompressed, binary_data, sizeof(binary_data)) != 0) {
        TEST_FAIL("Binary data mismatch");
        free(compressed);
        free(decompressed);
        return;
    }
    
    TEST_PASS();
    
    free(compressed);
    free(decompressed);
}

/**
 * Test version information functions
 */
void test_version_info(void) {
    TEST_START("Version information");
    
    // Get library version (using pointer parameters)
    int major, minor, patch;
    sevenzip_version(&major, &minor, &patch);
    
    if (major <= 0) {
        TEST_FAIL("Invalid version returned");
        return;
    }
    
    // Get LZMA SDK version string
    const char* lzma_version = sevenzip_lzma_version();
    if (!lzma_version || strlen(lzma_version) == 0) {
        TEST_FAIL("No LZMA version string returned");
        return;
    }
    
    // Verify LZMA version is "23.01"
    if (strcmp(lzma_version, "23.01") != 0) {
        TEST_FAIL("Unexpected LZMA version");
        return;
    }
    
    TEST_PASS();
}

/**
 * Test compress_bound function
 */
void test_compress_bound(void) {
    TEST_START("Compress bound calculation");
    
    // Test various sizes
    size_t test_sizes[] = {0, 1, 100, 1024, 10240, 1024*1024};
    int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int i = 0; i < num_tests; i++) {
        size_t bound = sevenzip_compress_bound(test_sizes[i]);
        
        if (test_sizes[i] > 0 && bound <= test_sizes[i]) {
            TEST_FAIL("Bound too small");
            return;
        }
        
        // Bound should be reasonable (not more than 2x + overhead)
        if (test_sizes[i] > 0 && bound > (test_sizes[i] * 2 + 1024)) {
            TEST_FAIL("Bound unreasonably large");
            return;
        }
    }
    
    TEST_PASS();
}

/**
 * Test error handling
 */
void test_error_handling(void) {
    TEST_START("Error handling");
    
    unsigned char buffer[100];
    size_t size = 100;
    
    // Test NULL input
    int result = sevenzip_compress_buffer(NULL, 100, buffer, &size, 5);
    if (result == 0) {
        TEST_FAIL("Should reject NULL input");
        return;
    }
    
    // Test NULL output
    result = sevenzip_compress_buffer(buffer, 100, NULL, &size, 5);
    if (result == 0) {
        TEST_FAIL("Should reject NULL output");
        return;
    }
    
    // Test NULL size
    result = sevenzip_compress_buffer(buffer, 100, buffer, NULL, 5);
    if (result == 0) {
        TEST_FAIL("Should reject NULL size");
        return;
    }
    
    // Test invalid compression level
    size = 100;
    result = sevenzip_compress_buffer(buffer, 100, buffer, &size, 99);
    if (result == 0) {
        TEST_FAIL("Should reject invalid compression level");
        return;
    }
    
    // Test zero size
    size = 100;
    result = sevenzip_compress_buffer(buffer, 0, buffer, &size, 5);
    if (result == 0) {
        TEST_FAIL("Should reject zero size");
        return;
    }
    
    TEST_PASS();
}

/**
 * Test buffer too small scenario
 */
void test_buffer_too_small(void) {
    TEST_START("Buffer too small handling");
    
    const char* data = "This is test data for compression.";
    size_t data_size = strlen(data) + 1;
    
    // Intentionally use buffer that's too small
    unsigned char small_buffer[10];
    size_t buffer_size = sizeof(small_buffer);
    
    int result = sevenzip_compress_buffer(
        (const unsigned char*)data,
        data_size,
        small_buffer,
        &buffer_size,
        5
    );
    
    // Should fail due to insufficient buffer
    if (result == 0) {
        TEST_FAIL("Should reject too-small buffer");
        return;
    }
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  Buffer Compression API Tests\n");
    printf("========================================\n\n");
    
    // Run all tests
    test_version_info();
    test_compress_bound();
    test_basic_compression();
    test_compression_levels();
    test_xz_format();
    test_binary_data();
    test_large_data();
    test_error_handling();
    test_buffer_too_small();
    
    // Print summary
    printf("\n========================================\n");
    printf("  Test Results\n");
    printf("========================================\n");
    printf("  Total:  %d tests\n", results.total_tests);
    printf("  Passed: %d tests (%.1f%%)\n", 
           results.passed_tests,
           (100.0 * results.passed_tests) / results.total_tests);
    printf("  Failed: %d tests\n", results.failed_tests);
    printf("========================================\n\n");
    
    return results.failed_tests == 0 ? 0 : 1;
}
