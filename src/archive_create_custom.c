/**
 * Multi-file Archive Creation using LZMA2
 * 
 * Creates archives with multiple files compressed using LZMA2.
 * Uses a simple custom format: Header + File Entries + Compressed Data
 * 
 * Archive Format:
 * - Magic: "7ZFF" (4 bytes)
 * - Version: 1 (1 byte)
 * - File Count: N (4 bytes)
 * - File Entries: N * Entry
 *   - Name Length (2 bytes)
 *   - Name (UTF-8)
 *   - Original Size (8 bytes)
 *   - Compressed Size (8 bytes)
 *   - Offset (8 bytes)
 *   - Timestamp (8 bytes)
 *   - Attributes (4 bytes)
 * - Compressed Data Blocks
 */

#include "../include/7z_ffi.h"
#include "Lzma2Enc.h"
#include "Alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEPARATOR '\\'
    #define STAT _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
    #include <unistd.h>
    #define PATH_SEPARATOR '/'
    #define STAT stat
#endif

#define ARCHIVE_MAGIC "7ZFF"
#define ARCHIVE_VERSION 1
#define MAX_PATH_LEN 4096

/* File entry in archive */
typedef struct {
    char* name;
    uint64_t original_size;
    uint64_t compressed_size;
    uint64_t offset;
    uint64_t timestamp;
    uint32_t attributes;
    unsigned char* compressed_data;
} ArchiveFileEntry;

/* Archive builder context */
typedef struct {
    ArchiveFileEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    CLzma2EncProps props;
} ArchiveBuilder;

/* Helper: Initialize archive builder */
static ArchiveBuilder* create_archive_builder(SevenZipCompressionLevel level) {
    ArchiveBuilder* builder = (ArchiveBuilder*)calloc(1, sizeof(ArchiveBuilder));
    if (!builder) return NULL;
    
    builder->entry_capacity = 16;
    builder->entries = (ArchiveFileEntry*)calloc(builder->entry_capacity, sizeof(ArchiveFileEntry));
    if (!builder->entries) {
        free(builder);
        return NULL;
    }
    
    /* Set compression properties */
    Lzma2EncProps_Init(&builder->props);
    switch (level) {
        case SEVENZIP_LEVEL_STORE:
            builder->props.lzmaProps.level = 0;
            builder->props.lzmaProps.dictSize = 1 << 16;
            break;
        case SEVENZIP_LEVEL_FASTEST:
            builder->props.lzmaProps.level = 1;
            builder->props.lzmaProps.dictSize = 1 << 18;
            break;
        case SEVENZIP_LEVEL_FAST:
            builder->props.lzmaProps.level = 3;
            builder->props.lzmaProps.dictSize = 1 << 20;
            break;
        case SEVENZIP_LEVEL_NORMAL:
            builder->props.lzmaProps.level = 5;
            builder->props.lzmaProps.dictSize = 1 << 23;
            builder->props.numBlockThreads_Max = 2;
            break;
        case SEVENZIP_LEVEL_MAXIMUM:
            builder->props.lzmaProps.level = 7;
            builder->props.lzmaProps.dictSize = 1 << 25;
            builder->props.numBlockThreads_Max = 2;
            break;
        case SEVENZIP_LEVEL_ULTRA:
            builder->props.lzmaProps.level = 9;
            builder->props.lzmaProps.dictSize = 1 << 26;
            builder->props.numBlockThreads_Max = 2;
            break;
        default:
            builder->props.lzmaProps.level = 5;
            builder->props.lzmaProps.dictSize = 1 << 23;
            builder->props.numBlockThreads_Max = 1;
    }
    Lzma2EncProps_Normalize(&builder->props);
    
    return builder;
}

/* Helper: Free archive builder */
static void free_archive_builder(ArchiveBuilder* builder) {
    if (!builder) return;
    
    for (size_t i = 0; i < builder->entry_count; i++) {
        if (builder->entries[i].name) free(builder->entries[i].name);
        if (builder->entries[i].compressed_data) free(builder->entries[i].compressed_data);
    }
    
    if (builder->entries) free(builder->entries);
    free(builder);
}

/* Helper: Compress file data using LZMA2 */
static SevenZipErrorCode compress_file_data(
    const unsigned char* input_data,
    size_t input_size,
    unsigned char** output_data,
    size_t* output_size,
    const CLzma2EncProps* props
) {
    CLzma2EncHandle encoder = Lzma2Enc_Create(&g_Alloc, &g_Alloc);
    if (!encoder) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    SRes res = Lzma2Enc_SetProps(encoder, props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(encoder);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    /* Allocate output buffer */
    size_t out_buf_size = input_size + input_size / 3 + 128;
    unsigned char* out_buf = (unsigned char*)malloc(out_buf_size);
    if (!out_buf) {
        Lzma2Enc_Destroy(encoder);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Compress */
    size_t out_size = out_buf_size;
    res = Lzma2Enc_Encode2(
        encoder,
        NULL, out_buf, &out_size,
        NULL, input_data, input_size,
        NULL
    );
    
    Lzma2Enc_Destroy(encoder);
    
    if (res != SZ_OK) {
        free(out_buf);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    *output_data = out_buf;
    *output_size = out_size;
    return SEVENZIP_OK;
}

/* Helper: Add file to archive */
static SevenZipErrorCode add_file_to_archive(
    ArchiveBuilder* builder,
    const char* file_path,
    const char* archive_name
) {
    /* Get file info */
    struct STAT st;
    if (STAT(file_path, &st) != 0) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return SEVENZIP_ERROR_INVALID_PARAM; /* Only regular files for now */
    }
    
    /* CRITICAL: Check file size to prevent memory exhaustion */
    /* Files > 1GB should use streaming API instead */
    #define MAX_SAFE_FILE_SIZE (1024ULL * 1024 * 1024)  /* 1 GB */
    if ((uint64_t)st.st_size > MAX_SAFE_FILE_SIZE) {
        fprintf(stderr, "WARNING: File '%s' is %.2f GB\n", 
                file_path, (double)st.st_size / 1e9);
        fprintf(stderr, "WARNING: This function loads entire files into memory!\n");
        fprintf(stderr, "WARNING: Use sevenzip_create_7z_streaming() for large files.\n");
        /* Continue anyway, but warn user */
    }
    
    /* Read file */
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(f);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    unsigned char* file_data = (unsigned char*)malloc(file_size);
    if (!file_data) {
        fclose(f);
        fprintf(stderr, "ERROR: Failed to allocate %ld bytes for file data\n", file_size);
        fprintf(stderr, "ERROR: File too large for in-memory compression.\n");
        fprintf(stderr, "ERROR: Use sevenzip_create_7z_streaming() instead.\n");
        return SEVENZIP_ERROR_MEMORY;
    }
    
    if (fread(file_data, 1, file_size, f) != (size_t)file_size) {
        free(file_data);
        fclose(f);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    fclose(f);
    
    /* Compress file data */
    unsigned char* compressed_data = NULL;
    size_t compressed_size = 0;
    SevenZipErrorCode result = compress_file_data(
        file_data, file_size,
        &compressed_data, &compressed_size,
        &builder->props
    );
    
    free(file_data);
    
    if (result != SEVENZIP_OK) {
        return result;
    }
    
    /* Expand entries array if needed */
    if (builder->entry_count >= builder->entry_capacity) {
        size_t new_capacity = builder->entry_capacity * 2;
        ArchiveFileEntry* new_entries = (ArchiveFileEntry*)realloc(
            builder->entries,
            new_capacity * sizeof(ArchiveFileEntry)
        );
        if (!new_entries) {
            free(compressed_data);
            return SEVENZIP_ERROR_MEMORY;
        }
        builder->entries = new_entries;
        builder->entry_capacity = new_capacity;
    }
    
    /* Add entry */
    ArchiveFileEntry* entry = &builder->entries[builder->entry_count];
    memset(entry, 0, sizeof(ArchiveFileEntry));
    
    entry->name = strdup(archive_name);
    entry->original_size = file_size;
    entry->compressed_size = compressed_size;
    entry->compressed_data = compressed_data;
    entry->timestamp = (uint64_t)st.st_mtime;
    entry->attributes = (uint32_t)st.st_mode;
    
    builder->entry_count++;
    
    return SEVENZIP_OK;
}

/* Helper: Write archive to file */
static SevenZipErrorCode write_archive(
    ArchiveBuilder* builder,
    const char* archive_path,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    FILE* f = fopen(archive_path, "wb");
    if (!f) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Write magic and version */
    fwrite(ARCHIVE_MAGIC, 1, 4, f);
    fwrite(&(char){ARCHIVE_VERSION}, 1, 1, f);
    
    /* Write file count */
    uint32_t file_count = (uint32_t)builder->entry_count;
    fwrite(&file_count, 4, 1, f);
    
    /* Calculate offsets */
    uint64_t current_offset = 0;
    for (size_t i = 0; i < builder->entry_count; i++) {
        builder->entries[i].offset = current_offset;
        current_offset += builder->entries[i].compressed_size;
    }
    
    /* Write file entries */
    for (size_t i = 0; i < builder->entry_count; i++) {
        ArchiveFileEntry* entry = &builder->entries[i];
        
        /* Name length and name */
        uint16_t name_len = (uint16_t)strlen(entry->name);
        fwrite(&name_len, 2, 1, f);
        fwrite(entry->name, 1, name_len, f);
        
        /* Sizes and offset */
        fwrite(&entry->original_size, 8, 1, f);
        fwrite(&entry->compressed_size, 8, 1, f);
        fwrite(&entry->offset, 8, 1, f);
        fwrite(&entry->timestamp, 8, 1, f);
        fwrite(&entry->attributes, 4, 1, f);
    }
    
    /* Write compressed data */
    for (size_t i = 0; i < builder->entry_count; i++) {
        fwrite(builder->entries[i].compressed_data, 1, 
               builder->entries[i].compressed_size, f);
        
        if (progress_callback) {
            progress_callback(i + 1, builder->entry_count, user_data);
        }
    }
    
    fclose(f);
    return SEVENZIP_OK;
}

/* Main function: Create multi-file archive */
SevenZipErrorCode sevenzip_create_archive(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !input_paths) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Count input files */
    size_t num_inputs = 0;
    while (input_paths[num_inputs] != NULL) {
        num_inputs++;
    }
    
    if (num_inputs == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Create archive builder */
    ArchiveBuilder* builder = create_archive_builder(level);
    if (!builder) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Add all files */
    SevenZipErrorCode result = SEVENZIP_OK;
    for (size_t i = 0; i < num_inputs; i++) {
        /* Extract filename from path */
        const char* filename = strrchr(input_paths[i], PATH_SEPARATOR);
        if (filename) {
            filename++; /* Skip separator */
        } else {
            filename = input_paths[i];
        }
        
        result = add_file_to_archive(builder, input_paths[i], filename);
        if (result != SEVENZIP_OK) {
            free_archive_builder(builder);
            return result;
        }
    }
    
    /* Write archive */
    result = write_archive(builder, archive_path, progress_callback, user_data);
    
    free_archive_builder(builder);
    return result;
}
