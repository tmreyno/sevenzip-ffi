/**
 * Multi-file Archive Extraction using LZMA2
 * 
 * Extracts archives created with sevenzip_create_archive()
 */

#include "../include/7z_ffi.h"
#include "Lzma2Dec.h"
#include "Alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <unistd.h>
#endif

#define ARCHIVE_MAGIC "7ZFF"
#define ARCHIVE_VERSION 1
#define IN_BUF_SIZE (1 << 16)
#define OUT_BUF_SIZE (1 << 16)

/* File entry structure (matches create format) */
typedef struct {
    char* name;
    uint64_t original_size;
    uint64_t compressed_size;
    uint64_t offset;
    uint64_t timestamp;
    uint32_t attributes;
} ArchiveEntry;

/* Helper: Create directory recursively */
static int create_directory_recursive(const char* path) {
    char tmp[1024];
    char* p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

/* Helper: Read archive header and entries */
static SevenZipErrorCode read_archive_header(
    FILE* f,
    ArchiveEntry** entries,
    uint32_t* entry_count
) {
    /* Read and verify magic */
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, ARCHIVE_MAGIC, 4) != 0) {
        return SEVENZIP_ERROR_INVALID_ARCHIVE;
    }
    
    /* Read and verify version */
    uint8_t version;
    if (fread(&version, 1, 1, f) != 1 || version != ARCHIVE_VERSION) {
        return SEVENZIP_ERROR_INVALID_ARCHIVE;
    }
    
    /* Read file count */
    uint32_t count;
    if (fread(&count, 4, 1, f) != 1 || count == 0) {
        return SEVENZIP_ERROR_INVALID_ARCHIVE;
    }
    
    /* Allocate entries */
    ArchiveEntry* ents = (ArchiveEntry*)calloc(count, sizeof(ArchiveEntry));
    if (!ents) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Read all entries */
    for (uint32_t i = 0; i < count; i++) {
        /* Read name length */
        uint16_t name_len;
        if (fread(&name_len, 2, 1, f) != 1) {
            for (uint32_t j = 0; j < i; j++) free(ents[j].name);
            free(ents);
            return SEVENZIP_ERROR_INVALID_ARCHIVE;
        }
        
        /* Read name */
        ents[i].name = (char*)malloc(name_len + 1);
        if (!ents[i].name || fread(ents[i].name, 1, name_len, f) != name_len) {
            for (uint32_t j = 0; j <= i; j++) if (ents[j].name) free(ents[j].name);
            free(ents);
            return SEVENZIP_ERROR_MEMORY;
        }
        ents[i].name[name_len] = '\0';
        
        /* Read metadata */
        if (fread(&ents[i].original_size, 8, 1, f) != 1 ||
            fread(&ents[i].compressed_size, 8, 1, f) != 1 ||
            fread(&ents[i].offset, 8, 1, f) != 1 ||
            fread(&ents[i].timestamp, 8, 1, f) != 1 ||
            fread(&ents[i].attributes, 4, 1, f) != 1) {
            for (uint32_t j = 0; j <= i; j++) free(ents[j].name);
            free(ents);
            return SEVENZIP_ERROR_INVALID_ARCHIVE;
        }
    }
    
    *entries = ents;
    *entry_count = count;
    return SEVENZIP_OK;
}

/* Helper: Decompress file from archive */
static SevenZipErrorCode extract_file_from_archive(
    FILE* archive_file,
    const ArchiveEntry* entry,
    const char* output_path,
    long data_start_pos
) {
    /* Seek to compressed data (absolute position) */
    if (fseek(archive_file, data_start_pos + entry->offset, SEEK_SET) != 0) {
        return SEVENZIP_ERROR_EXTRACT;
    }
    
    /* Open output file */
    FILE* out_file = fopen(output_path, "wb");
    if (!out_file) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Allocate buffers */
    Byte* in_buf = (Byte*)malloc(IN_BUF_SIZE);
    Byte* out_buf = (Byte*)malloc(OUT_BUF_SIZE);
    if (!in_buf || !out_buf) {
        if (in_buf) free(in_buf);
        if (out_buf) free(out_buf);
        fclose(out_file);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Read LZMA2 property byte (first byte of compressed data) */
    Byte prop;
    if (fread(&prop, 1, 1, archive_file) != 1) {
        free(in_buf);
        free(out_buf);
        fclose(out_file);
        return SEVENZIP_ERROR_EXTRACT;
    }
    
    /* Initialize LZMA2 decoder */
    CLzma2Dec decoder;
    Lzma2Dec_Construct(&decoder);
    SRes res = Lzma2Dec_Allocate(&decoder, prop, &g_Alloc);
    if (res != SZ_OK) {
        free(in_buf);
        free(out_buf);
        fclose(out_file);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    Lzma2Dec_Init(&decoder);
    
    /* Decompress data (remaining bytes after prop) */
    uint64_t remaining = entry->compressed_size - 1; // -1 for prop byte already read
    uint64_t out_processed = 0;
    SevenZipErrorCode result = SEVENZIP_OK;
    
    while (remaining > 0 && out_processed < entry->original_size) {
        size_t in_size = (remaining < IN_BUF_SIZE) ? remaining : IN_BUF_SIZE;
        in_size = fread(in_buf, 1, in_size, archive_file);
        if (in_size == 0) break;
        
        Byte* in_ptr = in_buf;
        size_t in_remaining = in_size;
        
        while (in_remaining > 0) {
            size_t out_size = OUT_BUF_SIZE;
            size_t in_processed = in_remaining;
            ELzmaStatus status;
            
            res = Lzma2Dec_DecodeToBuf(
                &decoder,
                out_buf, &out_size,
                in_ptr, &in_processed,
                LZMA_FINISH_ANY,
                &status
            );
            
            if (res != SZ_OK) {
                result = SEVENZIP_ERROR_COMPRESS;
                goto cleanup;
            }
            
            if (out_size > 0) {
                if (fwrite(out_buf, 1, out_size, out_file) != out_size) {
                    result = SEVENZIP_ERROR_EXTRACT;
                    goto cleanup;
                }
                out_processed += out_size;
            }
            
            in_ptr += in_processed;
            in_remaining -= in_processed;
            remaining -= in_processed;
            
            if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
                goto cleanup;
            }
        }
    }
    
cleanup:
    Lzma2Dec_Free(&decoder, &g_Alloc);
    free(in_buf);
    free(out_buf);
    fclose(out_file);
    
    if (result != SEVENZIP_OK) {
        remove(output_path);
    }
    
    return result;
}

/* Main function: Extract multi-file archive */
SevenZipErrorCode sevenzip_extract_archive(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !output_dir) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Open archive */
    FILE* archive_file = fopen(archive_path, "rb");
    if (!archive_file) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Read header and entries */
    ArchiveEntry* entries = NULL;
    uint32_t entry_count = 0;
    SevenZipErrorCode result = read_archive_header(archive_file, &entries, &entry_count);
    if (result != SEVENZIP_OK) {
        fclose(archive_file);
        return result;
    }
    
    /* Remember position after header */
    long data_start_pos = ftell(archive_file);
    
    /* Create output directory */
    create_directory_recursive(output_dir);
    
    /* Extract each file */
    for (uint32_t i = 0; i < entry_count; i++) {
        /* Build output path */
        char output_path[1024];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, entries[i].name);
        
        /* Create parent directory if needed */
        char* last_slash = strrchr(output_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            create_directory_recursive(output_path);
            *last_slash = '/';
        }
        
        /* Extract file */
        result = extract_file_from_archive(archive_file, &entries[i], output_path, data_start_pos);
        if (result != SEVENZIP_OK) {
            break;
        }
        
        /* Progress callback */
        if (progress_callback) {
            progress_callback(i + 1, entry_count, user_data);
        }
    }
    
    /* Cleanup */
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].name) free(entries[i].name);
    }
    free(entries);
    fclose(archive_file);
    
    return result;
}
