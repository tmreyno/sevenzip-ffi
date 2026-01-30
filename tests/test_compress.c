/**
 * Unit tests for compression functionality
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

/* Helper: Create a test file */
static int create_test_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    
    return written == len;
}

/* Helper: Check if file exists */
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Helper: Get file size */
static uint64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
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
    
    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);
    
    return content;
}

/* Test: Initialize library */
static int test_init() {
    SevenZipErrorCode result = sevenzip_init();
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Library initialization");
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Get version */
static int test_get_version() {
    sevenzip_init();
    
    const char* version = sevenzip_get_version();
    TEST_ASSERT(version != NULL, "Version string returned");
    TEST_ASSERT(strlen(version) > 0, "Version string not empty");
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Compress single file (Store level) */
static int test_compress_store() {
    sevenzip_init();
    
    /* Create test file */
    const char* test_content = "This is a test file for compression.\n";
    const char* input_file = "/tmp/test_compress_input.txt";
    const char* output_file = "/tmp/test_compress_output.lzma2";
    
    TEST_ASSERT(create_test_file(input_file, test_content), 
                "Create test input file");
    
    /* Compress with store level (no compression) */
    const char* inputs[] = {input_file, NULL};
    SevenZipErrorCode result = sevenzip_compress(
        output_file,
        inputs,
        SEVENZIP_LEVEL_STORE,
        NULL,
        NULL,
        NULL
    );
    
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Compression succeeds");
    TEST_ASSERT(file_exists(output_file), "Output file created");
    
    /* Cleanup */
    unlink(input_file);
    unlink(output_file);
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Compress single file (Normal level) */
static int test_compress_normal() {
    sevenzip_init();
    
    /* Create test file with repetitive content (compresses well) */
    const char* input_file = "/tmp/test_compress_input2.txt";
    const char* output_file = "/tmp/test_compress_output2.lzma2";
    
    FILE* f = fopen(input_file, "w");
    TEST_ASSERT(f != NULL, "Create test file");
    
    /* Write 100KB of repetitive data */
    for (int i = 0; i < 10000; i++) {
        fprintf(f, "This is line %d with repetitive content.\n", i);
    }
    fclose(f);
    
    uint64_t input_size = get_file_size(input_file);
    TEST_ASSERT(input_size > 0, "Test file has content");
    
    /* Compress with normal level */
    const char* inputs[] = {input_file, NULL};
    SevenZipErrorCode result = sevenzip_compress(
        output_file,
        inputs,
        SEVENZIP_LEVEL_NORMAL,
        NULL,
        NULL,
        NULL
    );
    
    TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Compression succeeds");
    TEST_ASSERT(file_exists(output_file), "Output file created");
    
    uint64_t output_size = get_file_size(output_file);
    TEST_ASSERT(output_size > 0, "Output file has content");
    TEST_ASSERT(output_size < input_size, "Output smaller than input");
    
    /* Cleanup */
    unlink(input_file);
    unlink(output_file);
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Compress with invalid parameters */
static int test_compress_invalid_params() {
    sevenzip_init();
    
    /* Test NULL archive path */
    const char* inputs[] = {"test.txt", NULL};
    SevenZipErrorCode result = sevenzip_compress(
        NULL,
        inputs,
        SEVENZIP_LEVEL_NORMAL,
        NULL,
        NULL,
        NULL
    );
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL archive path");
    
    /* Test NULL input paths */
    result = sevenzip_compress(
        "output.7z",
        NULL,
        SEVENZIP_LEVEL_NORMAL,
        NULL,
        NULL,
        NULL
    );
    TEST_ASSERT(result != SEVENZIP_OK, "Reject NULL input paths");
    
    sevenzip_cleanup();
    return 1;
}

/* Test: Stream options initialization */
static int test_stream_options_init() {
    SevenZipStreamOptions opts;
    memset(&opts, 0xFF, sizeof(opts));  /* Fill with garbage */
    
    sevenzip_stream_options_init(&opts);
    
    TEST_ASSERT(opts.num_threads >= 0, "Thread count valid");
    TEST_ASSERT(opts.dict_size >= 0, "Dict size valid");
    TEST_ASSERT(opts.solid == 0 || opts.solid == 1, "Solid flag valid");
    TEST_ASSERT(opts.split_size == 0, "Split size initialized");
    TEST_ASSERT(opts.chunk_size > 0, "Chunk size set");
    TEST_ASSERT(opts.delete_temp_on_error == 1, "Temp cleanup enabled");
    
    return 1;
}

/* Test: Compression levels */
static int test_compression_levels() {
    sevenzip_init();
    
    const char* input_file = "/tmp/test_levels_input.txt";
    
    /* Create test file */
    FILE* f = fopen(input_file, "w");
    TEST_ASSERT(f != NULL, "Create test file");
    for (int i = 0; i < 1000; i++) {
        fprintf(f, "Test data line %d\n", i);
    }
    fclose(f);
    
    uint64_t input_size = get_file_size(input_file);
    const char* inputs[] = {input_file, NULL};
    
    /* Test each compression level */
    SevenZipCompressionLevel levels[] = {
        SEVENZIP_LEVEL_STORE,
        SEVENZIP_LEVEL_FASTEST,
        SEVENZIP_LEVEL_NORMAL,
        SEVENZIP_LEVEL_MAXIMUM
    };
    
    for (int i = 0; i < 4; i++) {
        char output_file[256];
        snprintf(output_file, sizeof(output_file), 
                 "/tmp/test_level_%d.lzma2", levels[i]);
        
        SevenZipErrorCode result = sevenzip_compress(
            output_file,
            inputs,
            levels[i],
            NULL,
            NULL,
            NULL
        );
        
        TEST_ASSERT_EQUALS(SEVENZIP_OK, result, "Compression at level");
        TEST_ASSERT(file_exists(output_file), "Output file created");
        
        unlink(output_file);
    }
    
    unlink(input_file);
    sevenzip_cleanup();
    return 1;
}

/* Main test runner */
int main(int argc, char** argv) {
    printf("===========================================\n");
    printf("7z FFI SDK - Compression Unit Tests\n");
    printf("===========================================\n\n");
    
    /* Run all tests */
    RUN_TEST(test_init);
    RUN_TEST(test_get_version);
    RUN_TEST(test_compress_store);
    RUN_TEST(test_compress_normal);
    RUN_TEST(test_compress_invalid_params);
    RUN_TEST(test_stream_options_init);
    RUN_TEST(test_compression_levels);
    
    /* Print summary */
    printf("\n===========================================\n");
    printf("Test Results:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("===========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
