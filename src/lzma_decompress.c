/**
 * LZMA Decompression Implementation
 * 
 * Implements decompression of standalone LZMA and LZMA2 files
 */

#include "../include/7z_ffi.h"
#include "LzmaDec.h"
#include "Lzma2Dec.h"
#include "Alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LZMA_PROPS_SIZE 5
#define LZMA_HEADER_SIZE 13  // 5 bytes props + 8 bytes uncompressed size
#define IN_BUF_SIZE (1 << 16)  // 64KB input buffer
#define OUT_BUF_SIZE (1 << 16) // 64KB output buffer

/**
 * Read LZMA file header (props + uncompressed size)
 */
static SevenZipErrorCode read_lzma_header(FILE* in_file, Byte* props, UInt64* unpack_size) {
    Byte header[LZMA_HEADER_SIZE];
    
    if (fread(header, 1, LZMA_HEADER_SIZE, in_file) != LZMA_HEADER_SIZE) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // First 5 bytes are LZMA properties
    memcpy(props, header, LZMA_PROPS_SIZE);
    
    // Next 8 bytes are uncompressed size (little-endian)
    *unpack_size = 0;
    for (int i = 0; i < 8; i++) {
        *unpack_size |= ((UInt64)header[LZMA_PROPS_SIZE + i]) << (i * 8);
    }
    
    // If unpack size is 0xFFFFFFFFFFFFFFFF, it means unknown size
    // We'll need to decompress until end marker
    
    return SEVENZIP_OK;
}

/**
 * Decompress LZMA file using streaming decoder
 */
SevenZipErrorCode sevenzip_decompress_lzma(
    const char* lzma_path,
    const char* output_path,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!lzma_path || !output_path) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    FILE* in_file = NULL;
    FILE* out_file = NULL;
    Byte* in_buf = NULL;
    Byte* out_buf = NULL;
    CLzmaDec decoder;
    SevenZipErrorCode result = SEVENZIP_OK;
    SRes lzma_res;
    
    // Open input file
    in_file = fopen(lzma_path, "rb");
    if (!in_file) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Read LZMA header
    Byte props[LZMA_PROPS_SIZE];
    UInt64 unpack_size;
    result = read_lzma_header(in_file, props, &unpack_size);
    if (result != SEVENZIP_OK) {
        fclose(in_file);
        return result;
    }
    
    // Open output file
    out_file = fopen(output_path, "wb");
    if (!out_file) {
        fclose(in_file);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Allocate buffers
    in_buf = (Byte*)malloc(IN_BUF_SIZE);
    out_buf = (Byte*)malloc(OUT_BUF_SIZE);
    if (!in_buf || !out_buf) {
        result = SEVENZIP_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Initialize decoder
    LzmaDec_Construct(&decoder);
    lzma_res = LzmaDec_Allocate(&decoder, props, LZMA_PROPS_SIZE, &g_Alloc);
    if (lzma_res != SZ_OK) {
        result = SEVENZIP_ERROR_COMPRESS;
        goto cleanup;
    }
    
    LzmaDec_Init(&decoder);
    
    // Decompress in chunks
    UInt64 in_processed = 0;
    UInt64 out_processed = 0;
    int finished = 0;  // Use int instead of Bool
    
    while (!finished) {
        // Read input chunk
        size_t in_size = fread(in_buf, 1, IN_BUF_SIZE, in_file);
        if (in_size == 0) {
            break;
        }
        
        Byte* in_ptr = in_buf;
        size_t in_remaining = in_size;
        
        // Process input buffer
        while (in_remaining > 0 || finished) {
            size_t out_size = OUT_BUF_SIZE;
            size_t in_processed_chunk = in_remaining;
            ELzmaStatus status;
            
            lzma_res = LzmaDec_DecodeToBuf(
                &decoder,
                out_buf,
                &out_size,
                in_ptr,
                &in_processed_chunk,
                (in_size == 0) ? LZMA_FINISH_END : LZMA_FINISH_ANY,
                &status
            );
            
            if (lzma_res != SZ_OK) {
                result = SEVENZIP_ERROR_COMPRESS;
                goto cleanup;
            }
            
            // Write output
            if (out_size > 0) {
                if (fwrite(out_buf, 1, out_size, out_file) != out_size) {
                    result = SEVENZIP_ERROR_EXTRACT;
                    goto cleanup;
                }
                out_processed += out_size;
            }
            
            // Update input pointer
            in_ptr += in_processed_chunk;
            in_remaining -= in_processed_chunk;
            in_processed += in_processed_chunk;
            
            // Check if decompression is finished
            if (status == LZMA_STATUS_FINISHED_WITH_MARK ||
                (unpack_size != (UInt64)(Int64)-1 && out_processed >= unpack_size)) {
                finished = 1;
                break;
            }
            
            // If no more output and no more input, need more input
            if (out_size == 0 && in_remaining == 0) {
                break;
            }
            
            // Progress callback
            if (progress_callback && unpack_size != (UInt64)(Int64)-1) {
                progress_callback(out_processed, unpack_size, user_data);
            }
        }
    }
    
    // Final progress callback
    if (progress_callback) {
        progress_callback(out_processed, out_processed, user_data);
    }
    
cleanup:
    // Free decoder
    LzmaDec_Free(&decoder, &g_Alloc);
    
    // Free buffers
    if (in_buf) free(in_buf);
    if (out_buf) free(out_buf);
    
    // Close files
    if (in_file) fclose(in_file);
    if (out_file) fclose(out_file);
    
    // Remove output file on error
    if (result != SEVENZIP_OK && output_path) {
        remove(output_path);
    }
    
    return result;
}

/**
 * Decompress LZMA2 file using streaming decoder
 */
SevenZipErrorCode sevenzip_decompress_lzma2(
    const char* lzma2_path,
    const char* output_path,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!lzma2_path || !output_path) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    FILE* in_file = NULL;
    FILE* out_file = NULL;
    Byte* in_buf = NULL;
    Byte* out_buf = NULL;
    CLzma2Dec decoder;
    SevenZipErrorCode result = SEVENZIP_OK;
    SRes lzma_res;
    
    // Open input file
    in_file = fopen(lzma2_path, "rb");
    if (!in_file) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Read LZMA2 properties (1 byte)
    Byte prop;
    if (fread(&prop, 1, 1, in_file) != 1) {
        fclose(in_file);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Open output file
    out_file = fopen(output_path, "wb");
    if (!out_file) {
        fclose(in_file);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    // Allocate buffers
    in_buf = (Byte*)malloc(IN_BUF_SIZE);
    out_buf = (Byte*)malloc(OUT_BUF_SIZE);
    if (!in_buf || !out_buf) {
        result = SEVENZIP_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Initialize decoder
    Lzma2Dec_Construct(&decoder);
    lzma_res = Lzma2Dec_Allocate(&decoder, prop, &g_Alloc);
    if (lzma_res != SZ_OK) {
        result = SEVENZIP_ERROR_COMPRESS;
        goto cleanup;
    }
    
    Lzma2Dec_Init(&decoder);
    
    // Decompress in chunks
    UInt64 in_processed = 0;
    UInt64 out_processed = 0;
    int finished = 0;
    
    while (!finished) {
        // Read input chunk
        size_t in_size = fread(in_buf, 1, IN_BUF_SIZE, in_file);
        if (in_size == 0) {
            break;
        }
        
        Byte* in_ptr = in_buf;
        size_t in_remaining = in_size;
        
        // Process input buffer
        while (in_remaining > 0 || finished) {
            size_t out_size = OUT_BUF_SIZE;
            size_t in_processed_chunk = in_remaining;
            ELzmaStatus status;
            
            lzma_res = Lzma2Dec_DecodeToBuf(
                &decoder,
                out_buf,
                &out_size,
                in_ptr,
                &in_processed_chunk,
                (in_size == 0) ? LZMA_FINISH_END : LZMA_FINISH_ANY,
                &status
            );
            
            if (lzma_res != SZ_OK) {
                result = SEVENZIP_ERROR_COMPRESS;
                goto cleanup;
            }
            
            // Write output
            if (out_size > 0) {
                if (fwrite(out_buf, 1, out_size, out_file) != out_size) {
                    result = SEVENZIP_ERROR_EXTRACT;
                    goto cleanup;
                }
                out_processed += out_size;
            }
            
            // Update input pointer
            in_ptr += in_processed_chunk;
            in_remaining -= in_processed_chunk;
            in_processed += in_processed_chunk;
            
            // Check if decompression is finished
            if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
                finished = 1;
                break;
            }
            
            // If no more output and no more input, need more input
            if (out_size == 0 && in_remaining == 0) {
                break;
            }
            
            // Progress callback (we don't know total size for LZMA2)
            if (progress_callback) {
                progress_callback(out_processed, out_processed, user_data);
            }
        }
    }
    
    // Final progress callback
    if (progress_callback) {
        progress_callback(out_processed, out_processed, user_data);
    }
    
cleanup:
    // Free decoder
    Lzma2Dec_Free(&decoder, &g_Alloc);
    
    // Free buffers
    if (in_buf) free(in_buf);
    if (out_buf) free(out_buf);
    
    // Close files
    if (in_file) fclose(in_file);
    if (out_file) fclose(out_file);
    
    // Remove output file on error
    if (result != SEVENZIP_OK && output_path) {
        remove(output_path);
    }
    
    return result;
}
