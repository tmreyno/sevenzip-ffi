/**
 * 7z Archive Integrity Testing
 * 
 * Verify archive integrity without extracting files.
 * Validates CRCs, headers, and structure.
 */

#include "../include/7z_ffi.h"
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif

/* Test result structure */
typedef struct {
    int total_files;
    int tested_files;
    int errors;
    uint64_t total_bytes;
    uint64_t tested_bytes;
    char first_error[512];
} TestResult;

/* Multi-volume input stream (reuse from 7z_extract_split.c) */
typedef struct {
    ISeekInStream vt;
    FILE** volumes;
    int volume_count;
    uint64_t* volume_sizes;
    uint64_t* volume_offsets;
    uint64_t total_size;
    int current_volume;
    uint64_t current_pos;
} MultiVolumeInStream;

/* Forward declarations */
static int open_split_volumes(const char* base_path, MultiVolumeInStream* stream);
static void close_split_volumes(MultiVolumeInStream* stream);
static SRes MultiVolumeInStream_Read(ISeekInStreamPtr pp, void *buf, size_t *size);
static SRes MultiVolumeInStream_Seek(ISeekInStreamPtr pp, Int64 *pos, ESzSeek origin);

/* Seek callback for multi-volume stream */
static SRes MultiVolumeInStream_Seek(ISeekInStreamPtr pp, Int64 *pos, ESzSeek origin) {
    MultiVolumeInStream* p = (MultiVolumeInStream*)pp;
    
    uint64_t new_pos;
    switch (origin) {
        case SZ_SEEK_SET:
            new_pos = (uint64_t)*pos;
            break;
        case SZ_SEEK_CUR:
            new_pos = p->current_pos + (uint64_t)*pos;
            break;
        case SZ_SEEK_END:
            new_pos = p->total_size + (uint64_t)*pos;
            break;
        default:
            return SZ_ERROR_PARAM;
    }
    
    if (new_pos > p->total_size) {
        return SZ_ERROR_PARAM;
    }
    
    p->current_pos = new_pos;
    *pos = (Int64)new_pos;
    return SZ_OK;
}

/* Read callback for multi-volume stream */
static SRes MultiVolumeInStream_Read(ISeekInStreamPtr pp, void *buf, size_t *size) {
    MultiVolumeInStream* p = (MultiVolumeInStream*)pp;
    uint8_t* ptr = (uint8_t*)buf;
    size_t remaining = *size;
    size_t total_read = 0;
    
    while (remaining > 0 && p->current_pos < p->total_size) {
        // Find which volume contains current position
        int volume_idx = 0;
        for (int i = 0; i < p->volume_count; i++) {
            if (p->current_pos < p->volume_offsets[i + 1]) {
                volume_idx = i;
                break;
            }
        }
        
        // Calculate position within volume
        uint64_t volume_start = p->volume_offsets[volume_idx];
        uint64_t volume_pos = p->current_pos - volume_start;
        uint64_t volume_remaining = p->volume_sizes[volume_idx] - volume_pos;
        
        // Read from current volume
        size_t to_read = remaining;
        if (to_read > volume_remaining) {
            to_read = (size_t)volume_remaining;
        }
        
        FILE* vol = p->volumes[volume_idx];
        if (fseek(vol, (long)volume_pos, SEEK_SET) != 0) {
            *size = total_read;
            return SZ_ERROR_READ;
        }
        
        size_t bytes_read = fread(ptr, 1, to_read, vol);
        if (bytes_read == 0 && to_read > 0) {
            *size = total_read;
            return SZ_ERROR_READ;
        }
        
        ptr += bytes_read;
        remaining -= bytes_read;
        total_read += bytes_read;
        p->current_pos += bytes_read;
    }
    
    *size = total_read;
    return (total_read > 0) ? SZ_OK : SZ_ERROR_READ;
}

/* Open split volumes */
static int open_split_volumes(const char* base_path, MultiVolumeInStream* stream) {
    memset(stream, 0, sizeof(MultiVolumeInStream));
    stream->vt.Read = (SRes (*)(ISeekInStreamPtr, void *, size_t *))MultiVolumeInStream_Read;
    stream->vt.Seek = MultiVolumeInStream_Seek;
    
    // Try to open first volume
    FILE* first = fopen(base_path, "rb");
    if (!first) {
        // Try .001 extension
        char vol_path[1024];
        snprintf(vol_path, sizeof(vol_path), "%s.001", base_path);
        first = fopen(vol_path, "rb");
        if (!first) return 0;
        
        // Multi-volume archive
        stream->volumes = (FILE**)calloc(256, sizeof(FILE*));
        stream->volume_sizes = (uint64_t*)calloc(256, sizeof(uint64_t));
        stream->volume_offsets = (uint64_t*)calloc(257, sizeof(uint64_t));
        
        stream->volumes[0] = first;
        fseek(first, 0, SEEK_END);
        stream->volume_sizes[0] = (uint64_t)ftell(first);
        stream->volume_offsets[0] = 0;
        stream->volume_offsets[1] = stream->volume_sizes[0];
        stream->volume_count = 1;
        stream->total_size = stream->volume_sizes[0];
        
        // Open remaining volumes
        for (int i = 2; i <= 256; i++) {
            snprintf(vol_path, sizeof(vol_path), "%s.%03d", base_path, i);
            FILE* vol = fopen(vol_path, "rb");
            if (!vol) break;
            
            stream->volumes[i-1] = vol;
            fseek(vol, 0, SEEK_END);
            stream->volume_sizes[i-1] = (uint64_t)ftell(vol);
            stream->total_size += stream->volume_sizes[i-1];
            stream->volume_offsets[i] = stream->total_size;
            stream->volume_count = i;
        }
    } else {
        // Single volume
        stream->volumes = (FILE**)calloc(1, sizeof(FILE*));
        stream->volume_sizes = (uint64_t*)calloc(1, sizeof(uint64_t));
        stream->volume_offsets = (uint64_t*)calloc(2, sizeof(uint64_t));
        
        stream->volumes[0] = first;
        fseek(first, 0, SEEK_END);
        stream->volume_sizes[0] = (uint64_t)ftell(first);
        stream->volume_offsets[0] = 0;
        stream->volume_offsets[1] = stream->volume_sizes[0];
        stream->volume_count = 1;
        stream->total_size = stream->volume_sizes[0];
    }
    
    return 1;
}

/* Close split volumes */
static void close_split_volumes(MultiVolumeInStream* stream) {
    if (stream->volumes) {
        for (int i = 0; i < stream->volume_count; i++) {
            if (stream->volumes[i]) {
                fclose(stream->volumes[i]);
            }
        }
        free(stream->volumes);
    }
    free(stream->volume_sizes);
    free(stream->volume_offsets);
}

/**
 * Test archive integrity without extracting
 * @param archive_path Path to archive file
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional progress callback
 * @param user_data User data for progress callback
 * @return SEVENZIP_OK if archive is valid, error code otherwise
 */
SevenZipErrorCode sevenzip_test_archive(
    const char* archive_path,
    const char* password,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Initialize CRC tables
    CrcGenerateTable();
    
    // Open archive (possibly split volumes)
    MultiVolumeInStream in_stream = {0};
    if (!open_split_volumes(archive_path, &in_stream)) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Initialize 7z structures
    CLookToRead2 look_stream;
    CSzArEx db;
    ISzAlloc alloc_imp = {SzAlloc, SzFree};
    ISzAlloc alloc_temp_imp = {SzAllocTemp, SzFreeTemp};
    
    LookToRead2_CreateVTable(&look_stream, False);
    look_stream.buf = (Byte*)ISzAlloc_Alloc(&alloc_imp, (1 << 18)); // 256KB buffer
    if (!look_stream.buf) {
        close_split_volumes(&in_stream);
        return SEVENZIP_ERROR_MEMORY;
    }
    look_stream.bufSize = (1 << 18);
    look_stream.realStream = (ISeekInStreamPtr)&in_stream;
    LookToRead2_INIT(&look_stream);
    
    SzArEx_Init(&db);
    
    // Open and validate archive structure
    SRes res = SzArEx_Open(&db, &look_stream.vt, &alloc_imp, &alloc_temp_imp);
    
    if (res != SZ_OK) {
        SzArEx_Free(&db, &alloc_imp);
        ISzAlloc_Free(&alloc_imp, look_stream.buf);
        close_split_volumes(&in_stream);
        return (res == SZ_ERROR_NO_ARCHIVE) ? SEVENZIP_ERROR_INVALID_ARCHIVE :
               (res == SZ_ERROR_MEM) ? SEVENZIP_ERROR_MEMORY :
               SEVENZIP_ERROR_EXTRACT;
    }
    
    // Test result tracking
    TestResult result = {0};
    result.total_files = (int)db.NumFiles;
    
    // Calculate total bytes
    for (UInt32 i = 0; i < db.NumFiles; i++) {
        if (!SzArEx_IsDir(&db, i)) {
            result.total_bytes += SzArEx_GetFileSize(&db, i);
        }
    }
    
    // Test each file by extracting and verifying CRC
    UInt32 block_index = 0xFFFFFFFF;
    Byte* out_buffer = NULL;
    size_t out_buffer_size = 0;
    
    for (UInt32 i = 0; i < db.NumFiles; i++) {
        // Skip directories
        if (SzArEx_IsDir(&db, i)) {
            continue;
        }
        
        // Get file name for progress
        size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
        UInt16* temp = (UInt16*)malloc(len * sizeof(UInt16));
        char file_name[512] = {0};
        
        if (temp) {
            SzArEx_GetFileNameUtf16(&db, i, temp);
            // Convert UTF-16 to UTF-8 (simplified)
            for (size_t j = 0; j < len && j < 511; j++) {
                file_name[j] = (char)temp[j];
            }
            free(temp);
        }
        
        uint64_t file_size = SzArEx_GetFileSize(&db, i);
        
        // Report progress
        if (progress_callback) {
            progress_callback(
                result.tested_bytes,
                result.total_bytes,
                0,
                file_size,
                file_name,
                user_data
            );
        }
        
        // Extract file to memory (tests decompression and CRC)
        size_t offset = 0;
        size_t out_size_processed = 0;
        
        res = SzArEx_Extract(&db, &look_stream.vt, i,
                            &block_index, &out_buffer, &out_buffer_size,
                            &offset, &out_size_processed,
                            &alloc_imp, &alloc_temp_imp);
        
        if (res == SZ_OK) {
            result.tested_files++;
            result.tested_bytes += file_size;
        } else {
            result.errors++;
            if (result.first_error[0] == '\0') {
                snprintf(result.first_error, sizeof(result.first_error),
                        "Failed to test file: %s (error %d)", file_name, res);
            }
        }
    }
    
    // Final progress update
    if (progress_callback) {
        progress_callback(
            result.total_bytes,
            result.total_bytes,
            0, 0, "",
            user_data
        );
    }
    
    // Cleanup
    ISzAlloc_Free(&alloc_imp, out_buffer);
    SzArEx_Free(&db, &alloc_imp);
    ISzAlloc_Free(&alloc_imp, look_stream.buf);
    close_split_volumes(&in_stream);
    
    // Return result
    if (result.errors > 0) {
        fprintf(stderr, "Archive test failed: %s\n", result.first_error);
        return SEVENZIP_ERROR_EXTRACT;
    }
    
    return SEVENZIP_OK;
}
