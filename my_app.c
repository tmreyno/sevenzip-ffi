/*
 * Simple Example: Using sevenzip-ffi in Your Own Application
 * 
 * This demonstrates how to link and use the sevenzip-ffi library
 * in your own C program.
 */

#include <stdio.h>
#include <stdlib.h>
#include "7z_ffi.h"

int main(int argc, char* argv[]) {
    printf("sevenzip-ffi Example Application\n");
    printf("=================================\n\n");
    
    // Get library version
    int major, minor, patch;
    sevenzip_version(&major, &minor, &patch);
    printf("Library version: %d.%d.%d\n\n", major, minor, patch);
    
    if (argc < 3) {
        printf("Usage: %s <output.7z> <file1> [file2] [...]\n\n", argv[0]);
        printf("Example:\n");
        printf("  %s my_archive.7z document.txt image.png\n", argv[0]);
        return 1;
    }
    
    const char* archive_path = argv[1];
    
    // Build NULL-terminated array of input files
    int num_files = argc - 2;
    const char** input_files = (const char**)malloc((num_files + 1) * sizeof(char*));
    
    for (int i = 0; i < num_files; i++) {
        input_files[i] = argv[i + 2];
    }
    input_files[num_files] = NULL;
    
    printf("Creating archive: %s\n", archive_path);
    printf("Input files: %d\n", num_files);
    for (int i = 0; i < num_files; i++) {
        printf("  [%d] %s\n", i + 1, input_files[i]);
    }
    printf("\n");
    
    // Create the archive
    printf("Compressing...\n");
    SevenZipErrorCode result = sevenzip_create_7z(
        archive_path,
        input_files,
        SEVENZIP_LEVEL_NORMAL,  // Compression level 5
        NULL,                    // No password
        NULL,                    // No progress callback
        NULL                     // No user data
    );
    
    free(input_files);
    
    if (result == SEVENZIP_OK) {
        printf("✓ Success! Archive created: %s\n", archive_path);
        return 0;
    } else {
        printf("✗ Error: Failed to create archive (code: %d)\n", result);
        return 1;
    }
}
