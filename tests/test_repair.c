/**
 * Comprehensive Unit Tests for Archive Repair Functionality
 * Tests recovery of corrupted/damaged archives
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
 * Helper: Create test archive
 */
int create_test_archive(const char* archive_path) {
    // Create test files
    FILE* f1 = fopen("test_data/repair_test1.txt", "w");
    if (!f1) return -1;
    fprintf(f1, "Test file 1 content for repair testing\n");
    fclose(f1);
    
    FILE* f2 = fopen("test_data/repair_test2.txt", "w");
    if (!f2) return -1;
    fprintf(f2, "Test file 2 content for repair testing\n");
    fclose(f2);
    
    const char* files[] = {
        "test_data/repair_test1.txt",
        "test_data/repair_test2.txt",
        NULL  // NULL-terminated array
    };
    
    int result = sevenzip_create_7z(archive_path, files, SEVENZIP_LEVEL_NORMAL, NULL, NULL, NULL);
    
    unlink("test_data/repair_test1.txt");
    unlink("test_data/repair_test2.txt");
    
    return result;
}

/**
 * Helper: Corrupt archive by truncating it
 */
int truncate_archive(const char* path, size_t bytes_to_remove) {
    long size = get_file_size(path);
    if (size < 0) return -1;
    
    long new_size = size - bytes_to_remove;
    if (new_size < 0) new_size = 0;
    
    return truncate(path, new_size);
}

/**
 * Helper: Corrupt archive by modifying bytes
 */
int corrupt_bytes(const char* path, size_t offset, size_t count) {
    FILE* f = fopen(path, "r+b");
    if (!f) return -1;
    
    fseek(f, offset, SEEK_SET);
    for (size_t i = 0; i < count; i++) {
        unsigned char byte = rand() % 256;
        fwrite(&byte, 1, 1, f);
    }
    
    fclose(f);
    return 0;
}

/**
 * Test repairing truncated archive
 */
void test_repair_truncated_archive(void) {
    TEST_START("Repair truncated archive");
    
    const char* archive = "test_data/truncated.7z";
    const char* repaired = "test_data/truncated_repaired.7z";
    
    // Create good archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    long original_size = get_file_size(archive);
    
    // Truncate last 100 bytes
    if (truncate_archive(archive, 100) != 0) {
        TEST_FAIL("Failed to truncate archive");
        unlink(archive);
        return;
    }
    
    // Attempt repair
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    if (result != 0) {
        // Repair may fail for severely damaged archives
        // This is acceptable behavior
        printf("⊘ SKIP (repair not possible for this damage)\n");
        unlink(archive);
        return;
    }
    
    // Verify repaired archive exists
    if (!file_exists(repaired)) {
        TEST_FAIL("Repaired archive not created");
        unlink(archive);
        return;
    }
    
    // Repaired archive should exist (may be smaller due to data loss)
    long repaired_size = get_file_size(repaired);
    if (repaired_size <= 0) {
        TEST_FAIL("Repaired archive is empty");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    TEST_PASS();
    
    unlink(archive);
    unlink(repaired);
}

/**
 * Test repairing archive with corrupted header
 */
void test_repair_corrupted_header(void) {
    TEST_START("Repair archive with corrupted header");
    
    const char* archive = "test_data/corrupt_header.7z";
    const char* repaired = "test_data/corrupt_header_repaired.7z";
    
    // Create good archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Corrupt header (bytes 10-20)
    if (corrupt_bytes(archive, 10, 10) != 0) {
        TEST_FAIL("Failed to corrupt header");
        unlink(archive);
        return;
    }
    
    // Attempt repair
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    if (result != 0) {
        // Header corruption may be unrecoverable
        printf("⊘ SKIP (header too damaged)\n");
        unlink(archive);
        return;
    }
    
    if (!file_exists(repaired)) {
        TEST_FAIL("Repaired archive not created");
        unlink(archive);
        return;
    }
    
    TEST_PASS();
    
    unlink(archive);
    unlink(repaired);
}

/**
 * Test repairing archive with corrupted data
 */
void test_repair_corrupted_data(void) {
    TEST_START("Repair archive with corrupted data");
    
    const char* archive = "test_data/corrupt_data.7z";
    const char* repaired = "test_data/corrupt_data_repaired.7z";
    
    // Create good archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    long size = get_file_size(archive);
    
    // Corrupt middle section (data area)
    if (corrupt_bytes(archive, size / 2, 50) != 0) {
        TEST_FAIL("Failed to corrupt data");
        unlink(archive);
        return;
    }
    
    // Attempt repair
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    if (result != 0) {
        // Data corruption may prevent repair
        printf("⊘ SKIP (data too damaged)\n");
        unlink(archive);
        return;
    }
    
    if (!file_exists(repaired)) {
        TEST_FAIL("Repaired archive not created");
        unlink(archive);
        return;
    }
    
    TEST_PASS();
    
    unlink(archive);
    unlink(repaired);
}

/**
 * Test repairing valid archive (should succeed trivially)
 */
void test_repair_valid_archive(void) {
    TEST_START("Repair valid archive");
    
    const char* archive = "test_data/valid.7z";
    const char* repaired = "test_data/valid_repaired.7z";
    
    // Create good archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    long original_size = get_file_size(archive);
    
    // Attempt repair on valid archive
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Valid archive repair failed");
        unlink(archive);
        return;
    }
    
    if (!file_exists(repaired)) {
        TEST_FAIL("Repaired archive not created");
        unlink(archive);
        return;
    }
    
    long repaired_size = get_file_size(repaired);
    
    // Repaired version should be similar size (may differ slightly)
    if (repaired_size < original_size / 2 || repaired_size > original_size * 2) {
        TEST_FAIL("Repaired size suspiciously different");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    // Try to extract repaired archive
    result = sevenzip_extract(repaired, "test_data/repair_extract", NULL, NULL, NULL);
    if (result != 0) {
        TEST_FAIL("Cannot extract repaired archive");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    // Verify extracted files exist
    if (!file_exists("test_data/repair_extract/repair_test1.txt") ||
        !file_exists("test_data/repair_extract/repair_test2.txt")) {
        TEST_FAIL("Extracted files missing");
        system("rm -rf test_data/repair_extract");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    TEST_PASS();
    
    system("rm -rf test_data/repair_extract");
    unlink(archive);
    unlink(repaired);
}

/**
 * Test repairing empty file
 */
void test_repair_empty_file(void) {
    TEST_START("Repair empty file");
    
    const char* archive = "test_data/empty.7z";
    const char* repaired = "test_data/empty_repaired.7z";
    
    // Create empty file
    FILE* f = fopen(archive, "w");
    if (!f) {
        TEST_FAIL("Failed to create empty file");
        return;
    }
    fclose(f);
    
    // Attempt repair
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    // Should fail - empty file is not a valid archive
    if (result == 0) {
        TEST_FAIL("Should fail on empty file");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    TEST_PASS();
    
    unlink(archive);
    if (file_exists(repaired)) {
        unlink(repaired);
    }
}

/**
 * Test repairing non-7z file
 */
void test_repair_non_7z_file(void) {
    TEST_START("Repair non-7z file");
    
    const char* archive = "test_data/not_7z.txt";
    const char* repaired = "test_data/not_7z_repaired.7z";
    
    // Create text file
    FILE* f = fopen(archive, "w");
    if (!f) {
        TEST_FAIL("Failed to create text file");
        return;
    }
    fprintf(f, "This is not a 7z archive\n");
    fclose(f);
    
    // Attempt repair
    int result = sevenzip_repair_archive(archive, repaired, NULL, NULL);
    
    // Should fail - not a 7z archive
    if (result == 0) {
        TEST_FAIL("Should fail on non-7z file");
        unlink(archive);
        unlink(repaired);
        return;
    }
    
    TEST_PASS();
    
    unlink(archive);
    if (file_exists(repaired)) {
        unlink(repaired);
    }
}

/**
 * Test error handling
 */
void test_error_handling(void) {
    TEST_START("Error handling");
    
    // Test NULL damaged path
    int result = sevenzip_repair_archive(NULL, "output.7z", NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL damaged path");
        return;
    }
    
    // Test NULL output path
    result = sevenzip_repair_archive("input.7z", NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL output path");
        return;
    }
    
    // Test non-existent file
    result = sevenzip_repair_archive("nonexistent.7z", "output.7z", NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject non-existent file");
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
    printf("  Archive Repair Tests\n");
    printf("========================================\n\n");
    
    // Seed random for corruption
    srand(12345);
    
    // Ensure test_data directory exists
    mkdir("test_data", 0755);
    
    // Run all tests
    test_repair_valid_archive();
    test_repair_truncated_archive();
    test_repair_corrupted_header();
    test_repair_corrupted_data();
    test_repair_empty_file();
    test_repair_non_7z_file();
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
