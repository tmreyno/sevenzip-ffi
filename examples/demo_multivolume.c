/**
 * Demo: Creating Multi-Volume (Split) 7z Archives
 * 
 * This demonstrates how to create split archives that are compatible
 * with 7-Zip command-line tool.
 * 
 * Usage: ./demo_multivolume <output_base> <volume_size_mb> <file1> [file2...]
 * Example: ./demo_multivolume archive.7z 100 largefile.iso data.bin
 *          Creates: archive.7z.001, archive.7z.002, archive.7z.003, etc.
 */

#include "../include/7z_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Progress callback to show compression progress */
void progress_callback(uint64_t bytes_processed, uint64_t total_bytes,
                      uint64_t current_file_bytes, uint64_t current_file_total,
                      const char* current_file_name, void* user_data) {
    if (total_bytes > 0) {
        double percent = (100.0 * bytes_processed) / total_bytes;
        printf("\rProgress: %.1f%% (%llu / %llu bytes) - %s    ", 
               percent, 
               (unsigned long long)bytes_processed,
               (unsigned long long)total_bytes,
               current_file_name ? current_file_name : "");
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Multi-Volume 7z Archive Creator\n\n");
        printf("Usage: %s <output_base> <volume_size_mb> <file1> [file2...]\n\n", argv[0]);
        printf("Examples:\n");
        printf("  %s backup.7z 100 /large/file.iso\n", argv[0]);
        printf("  Creates: backup.7z.001, backup.7z.002, etc. (100MB each)\n\n");
        printf("  %s data.7z 50 file1.dat file2.dat file3.dat\n", argv[0]);
        printf("  Creates split archive with 50MB volumes\n\n");
        return 1;
    }

    const char* output_base = argv[1];
    double volume_size_mb = atof(argv[2]);
    
    if (volume_size_mb < 0.065) {
        fprintf(stderr, "Error: Volume size must be at least 0.065 MB (65KB)\n");
        return 1;
    }
    
    /* Convert MB to bytes */
    uint64_t volume_size_bytes = (uint64_t)(volume_size_mb * 1024.0 * 1024.0);
    
    /* Minimum volume size is 64KB */
    if (volume_size_bytes < 65536) {
        volume_size_bytes = 65536;
    }
    
    /* Build input file list (NULL-terminated) */
    int num_files = argc - 3;
    const char** input_files = (const char**)malloc((num_files + 1) * sizeof(char*));
    if (!input_files) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }
    
    for (int i = 0; i < num_files; i++) {
        input_files[i] = argv[i + 3];
    }
    input_files[num_files] = NULL;
    
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║     Multi-Volume 7z Archive Creator                     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Output base:    %s\n", output_base);
    printf("Volume size:    %.2f MB (%llu bytes)\n", 
           volume_size_mb, 
           (unsigned long long)volume_size_bytes);
    printf("Input files:    %d\n", num_files);
    for (int i = 0; i < num_files; i++) {
        printf("  [%d] %s\n", i + 1, input_files[i]);
    }
    printf("\n");
    
    /* Configure streaming options for multi-volume */
    SevenZipStreamOptions opts;
    sevenzip_stream_options_init(&opts);
    opts.split_size = volume_size_bytes;     /* Enable split archives */
    opts.chunk_size = 64 * 1024 * 1024;      /* 64MB chunks */
    opts.num_threads = 4;                    /* Use 4 threads */
    
    printf("Creating multi-volume archive...\n\n");
    
    /* Create the multi-volume archive */
    SevenZipErrorCode result = sevenzip_create_7z_streaming(
        output_base,
        input_files,
        SEVENZIP_LEVEL_NORMAL,    /* Compression level 5 (normal) */
        &opts,
        progress_callback,
        NULL
    );
    
    printf("\n\n");
    
    if (result == SEVENZIP_OK) {
        printf("✓ SUCCESS! Multi-volume archive created.\n\n");
        
        printf("Created volumes: %s.001, %s.002, ...\n", output_base, output_base);
        printf("\nTo extract:\n");
        printf("  7z x %s.001\n", output_base);
        printf("  (or use sevenzip_extract_split_archive)\n\n");
        
    } else {
        printf("✗ ERROR: Failed to create archive\n");
        printf("Error code: %d\n\n", result);
        
        switch (result) {
            case SEVENZIP_ERROR_OPEN_FILE:
                printf("Could not open one or more input files.\n");
                break;
            case SEVENZIP_ERROR_COMPRESS:
                printf("Compression error occurred.\n");
                break;
            case SEVENZIP_ERROR_MEMORY:
                printf("Out of memory.\n");
                break;
            case SEVENZIP_ERROR_INVALID_PARAM:
                printf("Invalid parameters.\n");
                break;
            default:
                printf("Unknown error.\n");
                break;
        }
    }
    
    free(input_files);
    return (result == SEVENZIP_OK) ? 0 : 1;
}
