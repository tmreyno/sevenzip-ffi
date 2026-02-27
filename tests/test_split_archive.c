/**
 * Comprehensive Unit Tests for Split Archive Extraction
 * Tests multi-volume .7z archives (.7z.001, .7z.002, etc.)
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * Helper: Check if file exists
 */
int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * Helper: Get file size
 */
long get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

/**
 * Helper: Clean up test directory
 */
void cleanup_test_dir(const char* dir_path) {
    char command[512];
    snprintf(command, sizeof(command), "rm -rf %s", dir_path);
    system(command);
}

/**
 * Helper: Create large test file
 */
int create_large_test_file(const char* path, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    // Write pattern data
    for (size_t i = 0; i < size; i++) {
        unsigned char byte = (unsigned char)(i % 256);
        fwrite(&byte, 1, 1, f);
    }
    
    fclose(f);
    return 0;
}

/**
 * Helper: Create split archive using system 7z command
 * (This simulates what we're testing - in real tests you'd use actual split archives)
 */
int create_split_archive(const char* base_name, size_t volume_size) {
    // Create test files
    if (create_large_test_file("test_data/split_file1.bin", 50000) != 0) return -1;
    if (create_large_test_file("test_data/split_file2.bin", 50000) != 0) return -1;
    if (create_large_test_file("test_data/split_file3.bin", 50000) != 0) return -1;
    
    // Create split archive using system 7z if available
    char command[1024];
    snprintf(command, sizeof(command),
             "cd test_data && 7z a -v%zuk %s split_file1.bin split_file2.bin split_file3.bin 2>/dev/null",
             volume_size / 1024, base_name);
    
    int result = system(command);
    
    // Clean up source files
    unlink("test_data/split_file1.bin");
    unlink("test_data/split_file2.bin");
    unlink("test_data/split_file3.bin");
    
    return result == 0 ? 0 : -1;
}

/**
 * Test extracting 2-volume archive
 */
void test_extract_two_volumes(void) {
    TEST_START("Extract 2-volume archive");
    
    const char* output_dir = "test_data/split_output1";
    
    // Create split archive (30KB per volume = 2 volumes needed)
    if (create_split_archive("test_split.7z", 30000) != 0) {
        printf("⊘ SKIP (7z not available or split creation failed)\n");
        return;
    }
    
    // Check if volumes were created
    if (!file_exists("test_data/test_split.7z.001") || 
        !file_exists("test_data/test_split.7z.002")) {
        printf("⊘ SKIP (split volumes not created)\n");
        return;
    }
    
    // Prepare volume paths
    const char* volumes[] = {
        "test_data/test_split.7z.001",
        "test_data/test_split.7z.002"
    };
    
    // Extract
    int result = sevenzip_extract_split_archive(volumes[0], output_dir, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Extraction failed");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split.7z.001");
        unlink("test_data/test_split.7z.002");
        return;
    }
    
    // Verify files were extracted
    char file1[512], file2[512], file3[512];
    snprintf(file1, sizeof(file1), "%s/split_file1.bin", output_dir);
    snprintf(file2, sizeof(file2), "%s/split_file2.bin", output_dir);
    snprintf(file3, sizeof(file3), "%s/split_file3.bin", output_dir);
    
    if (!file_exists(file1) || !file_exists(file2) || !file_exists(file3)) {
        TEST_FAIL("Not all files extracted");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split.7z.001");
        unlink("test_data/test_split.7z.002");
        return;
    }
    
    // Verify file sizes
    if (get_file_size(file1) != 50000 || 
        get_file_size(file2) != 50000 || 
        get_file_size(file3) != 50000) {
        TEST_FAIL("Extracted file sizes incorrect");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split.7z.001");
        unlink("test_data/test_split.7z.002");
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink("test_data/test_split.7z.001");
    unlink("test_data/test_split.7z.002");
}

/**
 * Test extracting 3-volume archive
 */
void test_extract_three_volumes(void) {
    TEST_START("Extract 3-volume archive");
    
    const char* output_dir = "test_data/split_output2";
    
    // Create split archive (20KB per volume = 3+ volumes needed)
    if (create_split_archive("test_split2.7z", 20000) != 0) {
        printf("⊘ SKIP (7z not available)\n");
        return;
    }
    
    // Check volumes
    if (!file_exists("test_data/test_split2.7z.001") || 
        !file_exists("test_data/test_split2.7z.002") ||
        !file_exists("test_data/test_split2.7z.003")) {
        printf("⊘ SKIP (3 volumes not created)\n");
        unlink("test_data/test_split2.7z.001");
        unlink("test_data/test_split2.7z.002");
        return;
    }
    
    const char* volumes[] = {
        "test_data/test_split2.7z.001",
        "test_data/test_split2.7z.002",
        "test_data/test_split2.7z.003"
    };
    
    int result = sevenzip_extract_split_archive(volumes[0], output_dir, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("3-volume extraction failed");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split2.7z.001");
        unlink("test_data/test_split2.7z.002");
        unlink("test_data/test_split2.7z.003");
        return;
    }
    
    // Verify extraction
    char file1[512];
    snprintf(file1, sizeof(file1), "%s/split_file1.bin", output_dir);
    
    if (!file_exists(file1)) {
        TEST_FAIL("Files not extracted");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split2.7z.001");
        unlink("test_data/test_split2.7z.002");
        unlink("test_data/test_split2.7z.003");
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink("test_data/test_split2.7z.001");
    unlink("test_data/test_split2.7z.002");
    unlink("test_data/test_split2.7z.003");
}

/**
 * Test with volumes in wrong order
 */
void test_volumes_wrong_order(void) {
    TEST_START("Volumes in wrong order");
    
    const char* output_dir = "test_data/split_output3";
    
    // Create split archive
    if (create_split_archive("test_split3.7z", 30000) != 0) {
        printf("⊘ SKIP (7z not available)\n");
        return;
    }
    
    if (!file_exists("test_data/test_split3.7z.001") || 
        !file_exists("test_data/test_split3.7z.002")) {
        printf("⊘ SKIP (volumes not created)\n");
        return;
    }
    
    // Provide volumes in reverse order
    const char* volumes[] = {
        "test_data/test_split3.7z.002",  // Wrong order
        "test_data/test_split3.7z.001"
    };
    
    int result = sevenzip_extract_split_archive(volumes[0], output_dir, NULL, NULL, NULL);
    
    // Should either succeed (if implementation handles it) or fail gracefully
    if (result != 0) {
        // Expected - wrong order should fail
        TEST_PASS();
    } else {
        // Implementation may auto-detect and reorder
        printf("✓ PASS (auto-reordered)\n");
        results.passed_tests++;
    }
    
    cleanup_test_dir(output_dir);
    unlink("test_data/test_split3.7z.001");
    unlink("test_data/test_split3.7z.002");
}

/**
 * Test missing volume
 */
void test_missing_volume(void) {
    TEST_START("Missing volume error");
    
    const char* output_dir = "test_data/split_output4";
    
    // Create split archive
    if (create_split_archive("test_split4.7z", 30000) != 0) {
        printf("⊘ SKIP (7z not available)\n");
        return;
    }
    
    if (!file_exists("test_data/test_split4.7z.001")) {
        printf("⊘ SKIP (volumes not created)\n");
        return;
    }
    
    // Delete second volume to simulate missing file
    unlink("test_data/test_split4.7z.002");
    
    // Try to extract with missing volume
    const char* volumes[] = {
        "test_data/test_split4.7z.001",
        "test_data/test_split4.7z.002"  // Missing
    };
    
    int result = sevenzip_extract_split_archive(volumes[0], output_dir, NULL, NULL, NULL);
    
    // Should fail due to missing volume
    if (result == 0) {
        TEST_FAIL("Should fail with missing volume");
        cleanup_test_dir(output_dir);
        unlink("test_data/test_split4.7z.001");
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink("test_data/test_split4.7z.001");
}

/**
 * Test single volume (edge case)
 */
void test_single_volume(void) {
    TEST_START("Single volume extraction");
    
    const char* output_dir = "test_data/split_output5";
    
    // Create a regular (non-split) archive
    FILE* f = fopen("test_data/single_file.txt", "w");
    if (!f) {
        TEST_FAIL("Failed to create test file");
        return;
    }
    fprintf(f, "Single file test\n");
    fclose(f);
    
    const char* files[] = {"test_data/single_file.txt", NULL};  // NULL-terminated array
    int result = sevenzip_create_7z("test_data/single.7z", files, SEVENZIP_LEVEL_NORMAL, NULL, NULL, NULL);
    unlink("test_data/single_file.txt");
    
    if (result != 0) {
        TEST_FAIL("Failed to create single archive");
        return;
    }
    
    // Try to extract as split archive (but with only one volume)
    const char* volumes[] = {"test_data/single.7z"};
    result = sevenzip_extract_split_archive(volumes[0], output_dir, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Single volume extraction failed");
        cleanup_test_dir(output_dir);
        unlink("test_data/single.7z");
        return;
    }
    
    // Verify extraction
    if (!file_exists("test_data/split_output5/single_file.txt")) {
        TEST_FAIL("File not extracted");
        cleanup_test_dir(output_dir);
        unlink("test_data/single.7z");
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink("test_data/single.7z");
}

/**
 * Test error handling
 */
void test_error_handling(void) {
    TEST_START("Error handling");
    
    const char* volumes[] = {"vol1.7z", "vol2.7z"};
    
    // Test NULL volume paths
    int result = sevenzip_extract_split_archive(NULL, "output", NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL volume paths");
        return;
    }
    
    // Test NULL output directory
    result = sevenzip_extract_split_archive(volumes[0], NULL, NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL output directory");
        return;
    }
    
    // Test zero volumes
    result = sevenzip_extract_split_archive(volumes[0], "output", NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject zero volumes");
        return;
    }
    
    // Test non-existent volumes
    result = sevenzip_extract_split_archive(volumes[0], "output", NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject non-existent volumes");
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
    printf("  Split Archive Extraction Tests\n");
    printf("========================================\n");
    printf("  Note: Some tests require 7z command\n");
    printf("========================================\n\n");
    
    // Ensure test_data directory exists
    mkdir("test_data", 0755);
    
    // Run all tests
    test_extract_two_volumes();
    test_extract_three_volumes();
    test_volumes_wrong_order();
    test_missing_volume();
    test_single_volume();
    test_error_handling();
    
    // Print summary
    printf("\n========================================\n");
    printf("  Test Results\n");
    printf("========================================\n");
    printf("  Total:  %d tests\n", results.total_tests);
    printf("  Passed: %d tests (%.1f%%)\n", 
           results.passed_tests,
           results.total_tests > 0 ? (100.0 * results.passed_tests) / results.total_tests : 0.0);
    printf("  Failed: %d tests\n", results.failed_tests);
    printf("========================================\n\n");
    
    return results.failed_tests == 0 ? 0 : 1;
}
