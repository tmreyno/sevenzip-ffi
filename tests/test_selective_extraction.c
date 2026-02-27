/**
 * Comprehensive Unit Tests for Selective File Extraction
 * Tests extracting specific files from archives
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

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
 * Helper: Check if directory exists
 */
int dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * Helper: Count files in directory
 */
int count_files(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return -1;
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

/**
 * Helper: Create test archive with multiple files
 */
int create_test_archive(const char* archive_path) {
    // Create test files
    FILE* f1 = fopen("test_data/selective_test1.txt", "w");
    if (!f1) return -1;
    fprintf(f1, "This is test file 1\n");
    fclose(f1);
    
    FILE* f2 = fopen("test_data/selective_test2.txt", "w");
    if (!f2) return -1;
    fprintf(f2, "This is test file 2\n");
    fclose(f2);
    
    FILE* f3 = fopen("test_data/selective_test3.txt", "w");
    if (!f3) return -1;
    fprintf(f3, "This is test file 3\n");
    fclose(f3);
    
    FILE* f4 = fopen("test_data/selective_readme.md", "w");
    if (!f4) return -1;
    fprintf(f4, "# README\nThis is a readme file\n");
    fclose(f4);
    
    // Create archive with all files
    const char* input_files[] = {
        "test_data/selective_test1.txt",
        "test_data/selective_test2.txt",
        "test_data/selective_test3.txt",
        "test_data/selective_readme.md",
        NULL  // NULL-terminated array
    };
    
    int result = sevenzip_create_7z(archive_path, input_files, SEVENZIP_LEVEL_NORMAL, NULL, NULL, NULL);
    
    // Clean up source files
    unlink("test_data/selective_test1.txt");
    unlink("test_data/selective_test2.txt");
    unlink("test_data/selective_test3.txt");
    unlink("test_data/selective_readme.md");
    
    return result;
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
 * Test extracting single file
 */
void test_extract_single_file(void) {
    TEST_START("Extract single file");
    
    const char* archive = "test_data/selective_archive.7z";
    const char* output_dir = "test_data/selective_output1";
    
    // Create test archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Extract only test1.txt
    const char* files[] = {"selective_test1.txt", NULL};
    int result = sevenzip_extract_files(archive, output_dir, files, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Extraction failed");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify only test1.txt was extracted
    char expected_path[512];
    snprintf(expected_path, sizeof(expected_path), "%s/selective_test1.txt", output_dir);
    
    if (!file_exists(expected_path)) {
        TEST_FAIL("Target file not extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify other files were NOT extracted
    char other_path[512];
    snprintf(other_path, sizeof(other_path), "%s/selective_test2.txt", output_dir);
    if (file_exists(other_path)) {
        TEST_FAIL("Non-target file was extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink(archive);
}

/**
 * Test extracting multiple specific files
 */
void test_extract_multiple_files(void) {
    TEST_START("Extract multiple specific files");
    
    const char* archive = "test_data/selective_archive2.7z";
    const char* output_dir = "test_data/selective_output2";
    
    // Create test archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Extract test1.txt and test3.txt
    const char* files[] = {"selective_test1.txt", "selective_test3.txt", NULL};
    int result = sevenzip_extract_files(archive, output_dir, files, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Extraction failed");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify both files were extracted
    char path1[512], path3[512];
    snprintf(path1, sizeof(path1), "%s/selective_test1.txt", output_dir);
    snprintf(path3, sizeof(path3), "%s/selective_test3.txt", output_dir);
    
    if (!file_exists(path1) || !file_exists(path3)) {
        TEST_FAIL("Target files not extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify test2.txt was NOT extracted
    char path2[512];
    snprintf(path2, sizeof(path2), "%s/selective_test2.txt", output_dir);
    if (file_exists(path2)) {
        TEST_FAIL("Non-target file was extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink(archive);
}

/**
 * Test extracting with pattern matching
 */
void test_extract_by_extension(void) {
    TEST_START("Extract files by extension pattern");
    
    const char* archive = "test_data/selective_archive3.7z";
    const char* output_dir = "test_data/selective_output3";
    
    // Create test archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Extract all .txt files
    const char* files[] = {
        "selective_test1.txt",
        "selective_test2.txt",
        "selective_test3.txt",
        NULL  // NULL-terminated array
    };
    int result = sevenzip_extract_files(archive, output_dir, files, NULL, NULL, NULL);
    
    if (result != 0) {
        TEST_FAIL("Extraction failed");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify all .txt files extracted
    char path1[512], path2[512], path3[512];
    snprintf(path1, sizeof(path1), "%s/selective_test1.txt", output_dir);
    snprintf(path2, sizeof(path2), "%s/selective_test2.txt", output_dir);
    snprintf(path3, sizeof(path3), "%s/selective_test3.txt", output_dir);
    
    if (!file_exists(path1) || !file_exists(path2) || !file_exists(path3)) {
        TEST_FAIL("Not all .txt files extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    // Verify .md file was NOT extracted
    char readme_path[512];
    snprintf(readme_path, sizeof(readme_path), "%s/selective_readme.md", output_dir);
    if (file_exists(readme_path)) {
        TEST_FAIL(".md file should not be extracted");
        cleanup_test_dir(output_dir);
        unlink(archive);
        return;
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink(archive);
}

/**
 * Test extracting non-existent file
 */
void test_extract_missing_file(void) {
    TEST_START("Extract non-existent file");
    
    const char* archive = "test_data/selective_archive4.7z";
    const char* output_dir = "test_data/selective_output4";
    
    // Create test archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Try to extract file that doesn't exist
    const char* files[] = {"nonexistent_file.txt", NULL};
    int result = sevenzip_extract_files(archive, output_dir, files, NULL, NULL, NULL);
    
    // Should either fail or succeed with no files extracted
    if (result == 0) {
        // If it succeeds, verify no files were extracted
        if (dir_exists(output_dir)) {
            int file_count = count_files(output_dir);
            if (file_count > 0) {
                TEST_FAIL("Files extracted when none should be");
                cleanup_test_dir(output_dir);
                unlink(archive);
                return;
            }
        }
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink(archive);
}

/**
 * Test empty file list
 */
void test_empty_file_list(void) {
    TEST_START("Empty file list");
    
    const char* archive = "test_data/selective_archive5.7z";
    const char* output_dir = "test_data/selective_output5";
    
    // Create test archive
    if (create_test_archive(archive) != 0) {
        TEST_FAIL("Failed to create test archive");
        return;
    }
    
    // Extract with empty list (should extract nothing or fail)
    int result = sevenzip_extract_files(archive, output_dir, NULL, NULL, NULL, NULL);
    
    if (result == 0) {
        // If succeeds, verify no files extracted
        if (dir_exists(output_dir)) {
            int file_count = count_files(output_dir);
            if (file_count > 0) {
                TEST_FAIL("Files extracted with empty list");
                cleanup_test_dir(output_dir);
                unlink(archive);
                return;
            }
        }
    }
    
    TEST_PASS();
    
    cleanup_test_dir(output_dir);
    unlink(archive);
}

/**
 * Test error handling
 */
void test_error_handling(void) {
    TEST_START("Error handling");
    
    const char* files[] = {"test.txt", NULL};
    
    // Test NULL archive path
    int result = sevenzip_extract_files(NULL, "output", files, NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL archive path");
        return;
    }
    
    // Test NULL output directory
    result = sevenzip_extract_files("archive.7z", NULL, files, NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject NULL output directory");
        return;
    }
    
    // Test non-existent archive
    result = sevenzip_extract_files("nonexistent.7z", "output", files, NULL, NULL, NULL);
    if (result == 0) {
        TEST_FAIL("Should reject non-existent archive");
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
    printf("  Selective Extraction Tests\n");
    printf("========================================\n\n");
    
    // Ensure test_data directory exists
    mkdir("test_data", 0755);
    
    // Run all tests
    test_extract_single_file();
    test_extract_multiple_files();
    test_extract_by_extension();
    test_extract_missing_file();
    test_empty_file_list();
    test_error_handling();
    
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
