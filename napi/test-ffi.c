// Simple test to verify FFI functions can be called
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Declare the FFI functions
extern const char* sevenzip_lzma_version(void);
extern size_t sevenzip_compress_bound(size_t input_size);

int main() {
    printf("LZMA Version: %s\n", sevenzip_lzma_version());
    printf("Compress bound for 1024 bytes: %zu\n", sevenzip_compress_bound(1024));
    return 0;
}
