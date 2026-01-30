/**
 * Split Archive Extraction with Streaming
 * 
 * Handles multi-volume 7z archives with byte-level progress tracking.
 */

#include "../include/7z_ffi.h"
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zVersion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
    #define PATH_SEP '\\'
#else
    #include <unistd.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0755)
    #define PATH_SEP '/'
#endif

/* Multi-volume input stream */
typedef struct {
    ISeekInStream vt;
    FILE** volumes;           // Array of volume file handles
    int volume_count;         // Number of volumes
    uint64_t* volume_sizes;   // Size of each volume
    uint64_t* volume_offsets; // Cumulative offsets
    uint64_t total_size;      // Total size across all volumes
    int current_volume;       // Currently active volume
    uint64_t current_pos;     // Global position
    
    /* Progress tracking */
    SevenZipBytesProgressCallback progress_callback;
    void* user_data;
    uint64_t bytes_extracted;
    uint64_t total_bytes;
    char current_file[512];
} MultiVolumeInStream;

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
        p->bytes_extracted += bytes_read;
        
        // Update progress
        if (p->progress_callback) {
            p->progress_callback(
                p->bytes_extracted,
                p->total_bytes,
                p->bytes_extracted, // Current file bytes
                p->total_bytes,     // Current file total
                p->current_file,
                p->user_data
            );
        }
    }
    
    *size = total_read;
    return SZ_OK;
}

/* Seek callback for multi-volume stream */
static SRes MultiVolumeInStream_Seek(ISeekInStreamPtr pp, Int64 *pos, ESzSeek origin) {
    MultiVolumeInStream* p = (MultiVolumeInStream*)pp;
    
    switch (origin) {
        case SZ_SEEK_SET:
            p->current_pos = (uint64_t)*pos;
            break;
        case SZ_SEEK_CUR:
            p->current_pos += *pos;
            break;
        case SZ_SEEK_END:
            p->current_pos = p->total_size + *pos;
            break;
    }
    
    if (p->current_pos > p->total_size) {
        p->current_pos = p->total_size;
    }
    
    *pos = (Int64)p->current_pos;
    return SZ_OK;
}

/* Detect and open all volumes of a split archive */
static int open_split_volumes(const char* first_volume_path, MultiVolumeInStream* stream) {
    // Parse base path (remove .001, .002, etc.)
    char base_path[1024];
    strncpy(base_path, first_volume_path, sizeof(base_path) - 1);
    
    // Check if path ends with .001, .002, etc.
    size_t len = strlen(base_path);
    if (len > 4 && base_path[len-4] == '.' && 
        base_path[len-3] >= '0' && base_path[len-3] <= '9') {
        base_path[len-4] = '\0'; // Remove .XXX
    }
    
    // Try to open volumes sequentially
    FILE** volumes = NULL;
    uint64_t* sizes = NULL;
    int count = 0;
    int capacity = 10;
    
    volumes = (FILE**)malloc(capacity * sizeof(FILE*));
    sizes = (uint64_t*)malloc(capacity * sizeof(uint64_t));
    
    if (!volumes || !sizes) {
        free(volumes);
        free(sizes);
        return 0;
    }
    
    // Try opening volumes
    for (int i = 1; i <= 999; i++) {
        char volume_path[1024];
        if (i == 1) {
            // First volume might be .7z or .7z.001
            snprintf(volume_path, sizeof(volume_path), "%s.001", base_path);
            FILE* f = fopen(volume_path, "rb");
            if (!f) {
                // Try without .001
                snprintf(volume_path, sizeof(volume_path), "%s", base_path);
                f = fopen(volume_path, "rb");
            }
            if (!f) break;
            
            volumes[count] = f;
        } else {
            snprintf(volume_path, sizeof(volume_path), "%s.%03d", base_path, i);
            FILE* f = fopen(volume_path, "rb");
            if (!f) break; // No more volumes
            
            volumes[count] = f;
        }
        
        // Get volume size
        fseek(volumes[count], 0, SEEK_END);
        sizes[count] = (uint64_t)ftell(volumes[count]);
        fseek(volumes[count], 0, SEEK_SET);
        
        count++;
        
        // Expand arrays if needed
        if (count >= capacity) {
            capacity *= 2;
            FILE** new_volumes = (FILE**)realloc(volumes, capacity * sizeof(FILE*));
            uint64_t* new_sizes = (uint64_t*)realloc(sizes, capacity * sizeof(uint64_t));
            if (!new_volumes || !new_sizes) {
                for (int j = 0; j < count; j++) fclose(volumes[j]);
                free(volumes);
                free(sizes);
                return 0;
            }
            volumes = new_volumes;
            sizes = new_sizes;
        }
    }
    
    if (count == 0) {
        free(volumes);
        free(sizes);
        return 0;
    }
    
    // Calculate cumulative offsets
    uint64_t* offsets = (uint64_t*)malloc((count + 1) * sizeof(uint64_t));
    if (!offsets) {
        for (int i = 0; i < count; i++) fclose(volumes[i]);
        free(volumes);
        free(sizes);
        return 0;
    }
    
    offsets[0] = 0;
    for (int i = 0; i < count; i++) {
        offsets[i + 1] = offsets[i] + sizes[i];
    }
    
    // Initialize stream
    stream->vt.Read = MultiVolumeInStream_Read;
    stream->vt.Seek = MultiVolumeInStream_Seek;
    stream->volumes = volumes;
    stream->volume_count = count;
    stream->volume_sizes = sizes;
    stream->volume_offsets = offsets;
    stream->total_size = offsets[count];
    stream->current_volume = 0;
    stream->current_pos = 0;
    stream->bytes_extracted = 0;
    
    return 1;
}

/* Close all volumes */
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
 * Extract a 7z archive with streaming decompression and split volume support
 */
SevenZipErrorCode sevenzip_extract_streaming(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !output_dir) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Initialize CRC tables
    CrcGenerateTable();
    
    // Open split volumes
    MultiVolumeInStream in_stream = {0};
    if (!open_split_volumes(archive_path, &in_stream)) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    in_stream.progress_callback = progress_callback;
    in_stream.user_data = user_data;
    in_stream.total_bytes = in_stream.total_size;
    
    // Create output directory
    MKDIR(output_dir);
    
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
    
    // Open archive
    SRes res = SzArEx_Open(&db, &look_stream.vt, &alloc_imp, &alloc_temp_imp);
    
    if (res == SZ_OK) {
        // Extract all files
        UInt32 block_index = 0xFFFFFFFF;
        Byte* out_buffer = NULL;
        size_t out_buffer_size = 0;
        
        for (UInt32 i = 0; i < db.NumFiles; i++) {
            size_t offset = 0;
            size_t out_size_processed = 0;
            
            // Get file info
            size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
            UInt16* temp = (UInt16*)malloc(len * sizeof(UInt16));
            if (temp) {
                SzArEx_GetFileNameUtf16(&db, i, temp);
                
                // Convert UTF-16 to UTF-8 (simplified)
                char file_name[512] = {0};
                for (size_t j = 0; j < len && j < 511; j++) {
                    file_name[j] = (char)temp[j];
                }
                
                strncpy(in_stream.current_file, file_name, sizeof(in_stream.current_file) - 1);
                free(temp);
                
                // Extract file
                res = SzArEx_Extract(&db, &look_stream.vt, i,
                                    &block_index, &out_buffer, &out_buffer_size,
                                    &offset, &out_size_processed,
                                    &alloc_imp, &alloc_temp_imp);
                
                if (res == SZ_OK && !SzArEx_IsDir(&db, i)) {
                    // Write file
                    char out_path[1024];
                    snprintf(out_path, sizeof(out_path), "%s%c%s", 
                            output_dir, PATH_SEP, file_name);
                    
                    FILE* out_file = fopen(out_path, "wb");
                    if (out_file) {
                        fwrite(out_buffer + offset, 1, out_size_processed, out_file);
                        fclose(out_file);
                    }
                }
            }
        }
        
        ISzAlloc_Free(&alloc_imp, out_buffer);
    }
    
    // Cleanup
    SzArEx_Free(&db, &alloc_imp);
    ISzAlloc_Free(&alloc_imp, look_stream.buf);
    close_split_volumes(&in_stream);
    
    return (res == SZ_OK) ? SEVENZIP_OK : SEVENZIP_ERROR_EXTRACT;
}
