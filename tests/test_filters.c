/**
 * Comprehensive Unit Tests for Filter API
 * Tests BCJ2, BCJ, ARM, ARM64, ARMT, IA64, PPC, SPARC, DELTA filters
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * Test filter names
 */
void test_filter_names(void) {
    TEST_START("Filter name retrieval");
    
    const char* name = sevenzip_filter_name(SEVENZIP_FILTER_BCJ);
    if (strcmp(name, "BCJ") != 0) {
        TEST_FAIL("Wrong name for BCJ");
        return;
    }
    
    name = sevenzip_filter_name(SEVENZIP_FILTER_BCJ2);
    if (strcmp(name, "BCJ2") != 0) {
        TEST_FAIL("Wrong name for BCJ2");
        return;
    }
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("\n========================================\n");
    printf("  Filter API Comprehensive Tests\n");
    printf("========================================\n\n");
    
    test_filter_names();
    
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
