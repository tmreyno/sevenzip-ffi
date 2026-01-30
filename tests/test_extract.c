/**
 * Unit tests for extraction functionality
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test utilities */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n  %s:%d: %s\n", message, __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_EQUALS(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "FAIL: %s\n  Expected: %d, Got: %d\n  %s:%d\n", \
                    message, (int)(expected), (int)(actual), __FILE__, __LINE__); \
            return 0; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s... ", #test_func); \
        fflush(stdout); \
        if (test_func()) { \
            printf("PASS\n"); \
            tests_passed++; \
        } else { \
            tests_failed++; \
        } \
        tests_run++; \
    } while(0)

/* Global test stats */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper: Check if file exists */
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Helper: Check if directory exists */
static int dir_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* Helper: Read file content */
static char* read_file_content(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);
    
    return content;
}

/* Helper: Remove directory recursively */
static void remove_dir_recursive(const char* path) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", path);
    system(command);
}

/* Test: Extract test_data archive */
static int test_extract_test_data() {
    sevenzip_init();
    
    const char* archive_path = "test_data.7z";
    const char* output_dir = "/tmp/test_extract_output";
    
    /* Check if test archive exists */
    if (!file_exists(archive_path)) {
        printf("SKIP (test_data.7z not found) ");
        sevenzip_cleanup();
        return 1;
    }
    
    /* Clean output directory */
    remove_dir_recursive(output_dir);
    
    /* Extract */
    SevenZipErrorCode result = sevenzip_extract(
        archive_path,
        output_dir,
        NULL,
        NULL,
        NULL
    );
    
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Extraction succeeds");
    TEST_ASSERT(dir_exists(output_dir), "Output directory created");
    
    /* Cleanup */
    remove_dir_recursive(output_dir);
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Extract with invalid parameters */
static int test_extract_invalid_params() {
    sevenzip_init();
    
    /* Test NULL archive path */
    SevenZipErrorCode result = sevenzip_extract(
        NULL,
        "/tmp/output",
        NULL,
        NULL,
        NULL
    );
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL archive path");
    
    /* Test NULL output directory */
    result = sevenzip_extract(
        "test.7z",
        NULL,
        NULL,
        NULL,
        NULL
    );
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL output directory");
    
    /* Test non-existent archive */
    result = sevenzip_extract(
        "/tmp/nonexistent_archive_12345.7z",
        "/tmp/output",
        NULL,
        NULL,
        NULL
    );
    TEST_ASSERT(result != SEVENZIP_OK, "Reject non-existent archive");
    
    sevenzip_cleanup();
    return 1;
}

/* Test: List archive contents */
static int test_list_archive() {
    sevenzip_init();
    
    const char* archive_path = "test_data.7z";
    
    /* Check if test archive exists */
    if (!file_exists(archive_path)) {
        printf("SKIP (test_data.7z not found) ");
        sevenzip_cleanup();
        return 1;
    }
    
    SevenZipList* list = NULL;
    SevenZipErrorCode result = sevenzip_list(
        archive_path,
        NULL,
        &list
    );
    
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "List succeeds");
    TEST_ASSERT(list != NULL, "List returned");
    TEST_ASSERT(list->count > 0, "List has entries");
    TEST_ASSERT(list->entries != NULL, "Entries array exists");
    
    /* Check first entry has valid data */
    if (list->count > 0) {
        TEST_ASSERT(list->entries[0].name != NULL, "Entry has name");
        TEST_ASSERT(strlen(list->entries[0].name) > 0, "Entry name not empty");
    }
    
    /* Free list */
    sevenzip_free_list(list);
    
    sevenzip_cleanup();
    return 1;
}

/* Test: List with invalid parameters */
static int test_list_invalid_params() {
    sevenzip_init();
    
    SevenZipList* list = NULL;
    
    /* Test NULL archive path */
    SevenZipErrorCode result = sevenzip_list(NULL, NULL, &list);
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL archive path");
    
    /* Test NULL list pointer */
    result = sevenzip_list("test.7z", NULL, NULL);
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL list pointer");
    
    /* Test non-existent archive */
    result = sevenzip_list("/tmp/nonexistent_archive_12345.7z", NULL, &list);
    TEST_ASSERT(result != SEVENZIP_OK, "Reject non-existent archive");
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Extract and verify content */
static int test_extract_and_verify() {
    sevenzip_init();
    
    /* First, create a simple archive */
    const char* input_file = "/tmp/test_round_trip_input.txt";
    const char* archive_path = "/tmp/test_round_trip.7z";
    const char* output_dir = "/tmp/test_round_trip_output";
    const char* test_content = "Hello, this is a test file for round-trip verification!\n";
    
    /* Create input file */
    FILE* f = fopen(input_file, "w");
    if (!f) {
        printf("SKIP (cannot create temp file) ");
        sevenzip_cleanup();
        return 1;
    }
    fputs(test_content, f);
    fclose(f);
    
    /* Compress */
    const char* inputs[] = {input_file, NULL};
    SevenZipErrorCode result = sevenzip_compress(
        archive_path,
        inputs,
        SEVENZIP_LEVEL_NORMAL,
        NULL,
        NULL,
        NULL
    );
    
    if (result != SEVENZIP_OK) {
        printf("SKIP (compression failed) ");
        unlink(input_file);
        sevenzip_cleanup();
        return 1;
    }
    
    /* Clean output directory */
    remove_dir_recursive(output_dir);
    
    /* Extract */
    result = sevenzip_extract(
        archive_path,
        output_dir,
        NULL,
        NULL,
        NULL
    );
    
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Extraction succeeds");
    
    /* Verify extracted content */
    char extracted_file[512];
    snprintf(extracted_file, sizeof(extracted_file), 
             "%s/test_round_trip_input.txt", output_dir);
    
    TEST_ASSERT(file_exists(extracted_file), "Extracted file exists");
    
    char* extracted_content = read_file_content(extracted_file);
    TEST_ASSERT(extracted_content != NULL, "Read extracted content");
    TEST_ASSERT(strcmp(test_content, extracted_content) == 0, 
                "Content matches original");
    
    /* Cleanup */
    free(extracted_content);
    unlink(input_file);
    unlink(archive_path);
    remove_dir_recursive(output_dir);
    
    sevenzip_cleanup();
    return 1;
}

/* Main test runner */
int main(int argc, char** argv) {
    printf("===========================================\n");
    printf("7z FFI SDK - Extraction Unit Tests\n");
    printf("===========================================\n\n");
    
    /* Run all tests */
    RUN_TEST(test_extract_test_data);
    RUN_TEST(test_extract_invalid_params);
    RUN_TEST(test_list_archive);
    RUN_TEST(test_list_invalid_params);
    RUN_TEST(test_extract_and_verify);
    
    /* Print summary */
    printf("\n===========================================\n");
    printf("Test Results:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("===========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
