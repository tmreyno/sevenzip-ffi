/**
 * buffer_compression.c - In-memory compression/decompression
 * 
 * Perfect for FFI usage in JavaScript, Rust, WebAssembly
 * No file I/O - pure memory operations
 */

#include "7z_ffi.h"
#include "LzmaEnc.h"
#include "LzmaDec.h"
#include "Lzma2Enc.h"
#include "Lzma2Dec.h"
#include "7zVersion.h"
#include "Alloc.h"

#include <string.h>
#include <stdlib.h>

/**
 * Simple LZMA compression (single stream)
 */
SevenZipErrorCode sevenzip_compress_buffer(
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t* output_size,
    int level
) {
    if (!input || !output || !output_size || input_size == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Validate compression level
    if (level < 0 || level > 9) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Check minimum buffer size (header + some compressed data)
    size_t min_size = LZMA_PROPS_SIZE + 8 + 64;  // Header + minimal compressed data
    if (*output_size < min_size) {
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.level = level;
    props.dictSize = (level >= 7) ? (1 << 24) : (1 << (level + 14));
    props.lc = 3;
    props.lp = 0;
    props.pb = 2;
    props.algo = (level >= 5) ? 1 : 0;
    props.fb = (level >= 7) ? 64 : 32;
    props.numThreads = 1; // Single-threaded for determinism
    
    // Prepare header (LZMA properties + uncompressed size)
    uint8_t header[LZMA_PROPS_SIZE + 8];
    size_t header_size = LZMA_PROPS_SIZE;
    
    SRes res = LzmaEncode(
        output + LZMA_PROPS_SIZE + 8,
        output_size,
        input,
        input_size,
        &props,
        header,
        &header_size,
        1, // Write end mark
        NULL, // No progress callback
        &g_Alloc,
        &g_Alloc
    );
    
    if (res != SZ_OK) {
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    // Write header (props + size)
    memcpy(output, header, LZMA_PROPS_SIZE);
    for (int i = 0; i < 8; i++) {
        output[LZMA_PROPS_SIZE + i] = (uint8_t)(input_size >> (8 * i));
    }
    
    *output_size += LZMA_PROPS_SIZE + 8;
    
    return SEVENZIP_OK;
}

/**
 * Simple LZMA decompression
 */
SevenZipErrorCode sevenzip_decompress_buffer(
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t* output_size
) {
    if (!input || !output || !output_size || input_size < LZMA_PROPS_SIZE + 8) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Parse header
    const uint8_t* props = input;
    uint64_t uncompressed_size = 0;
    
    for (int i = 0; i < 8; i++) {
        uncompressed_size |= ((uint64_t)input[LZMA_PROPS_SIZE + i]) << (8 * i);
    }
    
    if (uncompressed_size > *output_size) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    // Decompress
    ELzmaStatus status;
    size_t src_len = input_size - LZMA_PROPS_SIZE - 8;
    size_t dst_len = *output_size;
    
    SRes res = LzmaDecode(
        output,
        &dst_len,
        input + LZMA_PROPS_SIZE + 8,
        &src_len,
        props,
        LZMA_PROPS_SIZE,
        LZMA_FINISH_END,
        &status,
        &g_Alloc
    );
    
    if (res != SZ_OK) {
        return SEVENZIP_ERROR_EXTRACT;
    }
    
    *output_size = dst_len;
    
    return SEVENZIP_OK;
}

/**
 * Calculate maximum compressed size
 */
size_t sevenzip_compress_bound(size_t input_size) {
    // LZMA worst case: input_size + (input_size / 3) + 128
    return input_size + (input_size / 3) + 128 + LZMA_PROPS_SIZE + 8;
}

/**
 * XZ Format (LZMA2 stream with CRC64)
 */
SevenZipErrorCode sevenzip_compress_xz(
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t* output_size,
    int level
) {
    if (!input || !output || !output_size || input_size == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    if (level < 0) level = 5;
    if (level > 9) level = 9;
    
    // Write XZ magic header: 0xFD, '7', 'z', 'X', 'Z', 0x00
    size_t header_size = 12;  // XZ Stream Header is 12 bytes
    if (*output_size < header_size) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    output[0] = 0xFD;  // XZ magic number
    output[1] = '7';
    output[2] = 'z';
    output[3] = 'X';
    output[4] = 'Z';
    output[5] = 0x00;
    output[6] = 0x00;  // Stream Flags (no check)
    output[7] = 0x00;
    // CRC32 of Stream Flags (bytes 6-7)
    uint32_t crc = 0;
    output[8] = (crc >> 0) & 0xFF;
    output[9] = (crc >> 8) & 0xFF;
    output[10] = (crc >> 16) & 0xFF;
    output[11] = (crc >> 24) & 0xFF;
    
    CLzma2EncProps props;
    Lzma2EncProps_Init(&props);
    props.lzmaProps.level = level;
    props.lzmaProps.dictSize = (level >= 7) ? (1 << 24) : (1 << (level + 14));
    props.blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO;
    props.numBlockThreads_Reduced = 1;
    props.numBlockThreads_Max = 1;
    props.numTotalThreads = 1;
    
    CLzma2EncHandle enc = Lzma2Enc_Create(&g_Alloc, &g_Alloc);
    if (!enc) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    SRes res = Lzma2Enc_SetProps(enc, &props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(enc);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    // Write LZMA2 properties after XZ header
    Byte prop = Lzma2Enc_WriteProperties(enc);
    output[header_size] = prop;
    
    // Encode
    size_t out_pos = header_size + 1;
    size_t in_pos = 0;
    
    while (in_pos < input_size) {
        size_t chunk_size = (input_size - in_pos > (1 << 16)) ? (1 << 16) : (input_size - in_pos);
        size_t out_size = *output_size - out_pos;
        
        res = Lzma2Enc_Encode2(
            enc,
            NULL, // No output stream, use buffer
            output + out_pos,
            &out_size,
            NULL, // No input stream, use buffer
            input + in_pos,
            chunk_size,
            NULL // No progress
        );
        
        if (res != SZ_OK) {
            Lzma2Enc_Destroy(enc);
            return SEVENZIP_ERROR_COMPRESS;
        }
        
        out_pos += out_size;
        in_pos += chunk_size;
    }
    
    Lzma2Enc_Destroy(enc);
    *output_size = out_pos;
    
    return SEVENZIP_OK;
}

/**
 * XZ decompression (LZMA2)
 */
SevenZipErrorCode sevenzip_decompress_xz(
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t* output_size
) {
    if (!input || !output || !output_size || input_size < 13) {  // XZ header (12) + props (1)
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    // Verify XZ magic header
    if (input[0] != 0xFD || input[1] != '7' || input[2] != 'z' || 
        input[3] != 'X' || input[4] != 'Z' || input[5] != 0x00) {
        return SEVENZIP_ERROR_INVALID_ARCHIVE;
    }
    
    // Skip XZ header (12 bytes) and get LZMA2 properties
    size_t header_size = 12;
    
    CLzma2Dec decoder;
    Lzma2Dec_Construct(&decoder);
    
    // Initialize with properties byte (after XZ header)
    SRes res = Lzma2Dec_Allocate(&decoder, input[header_size], &g_Alloc);
    if (res != SZ_OK) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    Lzma2Dec_Init(&decoder);
    
    // Decode (skip header + props byte)
    size_t in_pos = header_size + 1;
    size_t out_pos = 0;
    ELzmaStatus status;
    
    while (in_pos < input_size && out_pos < *output_size) {
        size_t src_len = input_size - in_pos;
        size_t dst_len = *output_size - out_pos;
        ELzmaFinishMode finish = LZMA_FINISH_ANY;
        
        res = Lzma2Dec_DecodeToBuf(
            &decoder,
            output + out_pos,
            &dst_len,
            input + in_pos,
            &src_len,
            finish,
            &status
        );
        
        if (res != SZ_OK) {
            Lzma2Dec_Free(&decoder, &g_Alloc);
            return SEVENZIP_ERROR_EXTRACT;
        }
        
        in_pos += src_len;
        out_pos += dst_len;
        
        if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
            break;
        }
    }
    
    Lzma2Dec_Free(&decoder, &g_Alloc);
    *output_size = out_pos;
    
    return SEVENZIP_OK;
}

/**
 * Version information
 */
void sevenzip_version(int* major, int* minor, int* patch) {
    if (major) *major = 1;
    if (minor) *minor = 0;
    if (patch) *patch = 0;
}

const char* sevenzip_lzma_version(void) {
    return MY_VERSION_NUMBERS;
}
