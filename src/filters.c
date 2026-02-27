/**
 * filters.c - 7z filter implementations (BCJ, BCJ2, ARM, Delta, etc.)
 * 
 * Provides filter configuration and application for 7z archives.
 * Filters improve compression ratio for specific data types (executables, etc.)
 */

#include "7z_ffi.h"
#include "Bcj2.h"
#include "Bra.h"
#include "Delta.h"
#include "7zTypes.h"

#include <stdlib.h>
#include <string.h>

/**
 * Apply BCJ filter (x86 executable filter)
 * Converts relative jumps/calls to absolute addresses for better compression
 */
static size_t apply_bcj_filter(Byte* data, size_t size, UInt32 ip) {
    UInt32 state = Z7_BRANCH_CONV_ST_X86_STATE_INIT_VAL;
    Byte* result = Z7_BRANCH_CONV_ST_ENC(X86)(data, (SizeT)size, ip, &state);
    return (size_t)(result - data);
}

/**
 * Apply BCJ2 filter (x86 executable filter with 4 streams)
 * More sophisticated than BCJ, splits data into 4 streams for better compression
 * 
 * NOTE: BCJ2 requires complex stream separation and is not suitable for
 * simple in-place filtering. It should be integrated directly into the
 * 7z compression pipeline. Use BCJ filter instead for simpler cases.
 */
static SevenZipErrorCode apply_bcj2_filter(
    const Byte* src, size_t src_size,
    Byte* dest, size_t* dest_size,
    Byte* main_stream, size_t* main_size,
    Byte* call_stream, size_t* call_size,
    Byte* jump_stream, size_t* jump_size
) {
    // Suppress unused parameter warnings
    (void)src; (void)src_size;
    (void)dest; (void)dest_size;
    (void)main_stream; (void)main_size;
    (void)call_stream; (void)call_size;
    (void)jump_stream; (void)jump_size;
    
    // BCJ2 requires integration with compression pipeline
    return SEVENZIP_ERROR_NOT_IMPLEMENTED;
}

/**
 * Apply ARM filter (ARM executable filter)
 */
static size_t apply_arm_filter(Byte* data, size_t size, UInt32 ip) {
    Byte* result = Z7_BRANCH_CONV_ENC(ARM)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply ARM64 filter (ARM64 executable filter)
 */
static size_t apply_arm64_filter(Byte* data, size_t size, UInt32 ip) {
    Byte* result = Z7_BRANCH_CONV_ENC(ARM64)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply ARMT filter (ARM Thumb executable filter)
 */
static size_t apply_armt_filter(Byte* data, size_t size, UInt32 ip) {
    Byte* result = Z7_BRANCH_CONV_ENC(ARMT)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply IA64 filter (Intel Itanium executable filter)
 */
static size_t apply_ia64_filter(Byte* data, size_t size, UInt32 ip) {
    Byte* result = Z7_BRANCH_CONV_ENC(IA64)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply PPC filter (PowerPC executable filter)
 */
static size_t apply_ppc_filter(Byte* data, size_t size, UInt32 ip, int encoding) {
    (void)encoding;  // PPC doesn't use encoding parameter in this API
    Byte* result = Z7_BRANCH_CONV_ENC(PPC)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply SPARC filter (SPARC executable filter)
 */
static size_t apply_sparc_filter(Byte* data, size_t size, UInt32 ip) {
    Byte* result = Z7_BRANCH_CONV_ENC(SPARC)(data, (SizeT)size, ip);
    return (size_t)(result - data);
}

/**
 * Apply Delta filter
 * Calculates differences between adjacent bytes for better compression of data
 * with predictable patterns (audio, images, etc.)
 */
static void apply_delta_filter(Byte* data, size_t size, unsigned delta) {
    if (delta == 0 || delta > 256) return;
    
    // Simple delta encoding: each byte becomes (byte[i] - byte[i-delta])
    for (size_t i = size - 1; i >= delta; i--) {
        data[i] -= data[i - delta];
    }
}

/**
 * Apply filter to data based on filter options
 * 
 * @param data Input/output buffer containing data to filter
 * @param size Size of data buffer
 * @param filter Filter options (type, delta, alignment, etc.)
 * @return SEVENZIP_OK on success, error code otherwise
 */
SevenZipErrorCode sevenzip_apply_filter(
    Byte* data,
    size_t size,
    const SevenZipFilterOptions* filter
) {
    if (!data || !filter || size == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    UInt32 ip = 0;  // Instruction pointer (typically 0 for encoding)
    
    switch (filter->type) {
        case SEVENZIP_FILTER_NONE:
            // No filtering
            break;
            
        case SEVENZIP_FILTER_BCJ:
            apply_bcj_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_BCJ2:
            // BCJ2 requires special handling with multiple streams
            // This is simplified - full implementation would need stream separation
            return SEVENZIP_ERROR_NOT_IMPLEMENTED;
            
        case SEVENZIP_FILTER_ARM:
            apply_arm_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_ARM64:
            apply_arm64_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_ARMT:
            apply_armt_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_IA64:
            apply_ia64_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_PPC:
            apply_ppc_filter(data, size, ip, 1);
            break;
            
        case SEVENZIP_FILTER_SPARC:
            apply_sparc_filter(data, size, ip);
            break;
            
        case SEVENZIP_FILTER_DELTA:
            if (filter->delta > 0 && filter->delta <= 256) {
                apply_delta_filter(data, size, filter->delta);
            } else {
                return SEVENZIP_ERROR_INVALID_PARAM;
            }
            break;
            
        default:
            return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    return SEVENZIP_OK;
}

/**
 * Reverse filter application (for decompression)
 */
SevenZipErrorCode sevenzip_reverse_filter(
    Byte* data,
    size_t size,
    const SevenZipFilterOptions* filter
) {
    if (!data || !filter || size == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    UInt32 ip = 0;
    
    switch (filter->type) {
        case SEVENZIP_FILTER_NONE:
            break;
            
        case SEVENZIP_FILTER_BCJ: {
            UInt32 state = Z7_BRANCH_CONV_ST_X86_STATE_INIT_VAL;
            Z7_BRANCH_CONV_ST_DEC(X86)(data, (SizeT)size, ip, &state);
            break;
        }
            
        case SEVENZIP_FILTER_ARM:
            Z7_BRANCH_CONV_DEC(ARM)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_ARM64:
            Z7_BRANCH_CONV_DEC(ARM64)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_ARMT:
            Z7_BRANCH_CONV_DEC(ARMT)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_IA64:
            Z7_BRANCH_CONV_DEC(IA64)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_PPC:
            Z7_BRANCH_CONV_DEC(PPC)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_SPARC:
            Z7_BRANCH_CONV_DEC(SPARC)(data, (SizeT)size, ip);
            break;
            
        case SEVENZIP_FILTER_DELTA: {
            if (filter->delta > 0 && filter->delta <= 256) {
                // Simple delta decoding: each byte becomes (byte[i] + byte[i-delta])
                for (size_t i = filter->delta; i < size; i++) {
                    data[i] += data[i - filter->delta];
                }
            } else {
                return SEVENZIP_ERROR_INVALID_PARAM;
            }
            break;
        }
            
        case SEVENZIP_FILTER_BCJ2:
            // BCJ2 decode requires special handling
            return SEVENZIP_ERROR_NOT_IMPLEMENTED;
            
        default:
            return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    return SEVENZIP_OK;
}

/**
 * Detect appropriate filter for data type
 * Analyzes data to suggest best filter for compression
 */
SevenZipFilterType sevenzip_detect_filter(const Byte* data, size_t size) {
    if (!data || size < 4) {
        return SEVENZIP_FILTER_NONE;
    }
    
    // Check for executable file signatures
    
    // Windows PE (x86/x64)
    if (size > 64 && data[0] == 'M' && data[1] == 'Z') {
        return SEVENZIP_FILTER_BCJ;
    }
    
    // ELF (Linux/Unix)
    if (size > 16 && data[0] == 0x7F && data[1] == 'E' && 
        data[2] == 'L' && data[3] == 'F') {
        // Check architecture from ELF header
        if (size > 18) {
            unsigned char machine = data[18];
            switch (machine) {
                case 0x03:  // EM_386 (x86)
                case 0x3E:  // EM_X86_64 (x86-64)
                    return SEVENZIP_FILTER_BCJ;
                case 0x28:  // EM_ARM
                    return SEVENZIP_FILTER_ARM;
                case 0xB7:  // EM_AARCH64
                    return SEVENZIP_FILTER_ARM64;
                case 0x14:  // EM_PPC
                    return SEVENZIP_FILTER_PPC;
                case 0x32:  // EM_IA_64
                    return SEVENZIP_FILTER_IA64;
                case 0x2B:  // EM_SPARC
                    return SEVENZIP_FILTER_SPARC;
            }
        }
    }
    
    // Mach-O (macOS)
    if (size > 4) {
        UInt32 magic = *(UInt32*)data;
        if (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||  // 32/64-bit
            magic == 0xCEFAEDFE || magic == 0xCFFAEDFE) {  // Reversed
            return SEVENZIP_FILTER_BCJ;  // x86/x64 Mach-O
        }
    }
    
    // Check for patterns suggesting Delta filter
    // (audio/image data with predictable byte sequences)
    if (size > 256) {
        int delta_score = 0;
        for (size_t i = 1; i < 256; i++) {
            int diff = (int)data[i] - (int)data[i-1];
            if (diff >= -16 && diff <= 16) {
                delta_score++;
            }
        }
        // If >80% of bytes have small deltas, suggest Delta filter
        if (delta_score > 200) {
            return SEVENZIP_FILTER_DELTA;
        }
    }
    
    return SEVENZIP_FILTER_NONE;
}

/**
 * Get human-readable name for filter type
 */
const char* sevenzip_filter_name(SevenZipFilterType type) {
    switch (type) {
        case SEVENZIP_FILTER_NONE:   return "None";
        case SEVENZIP_FILTER_BCJ:    return "BCJ";
        case SEVENZIP_FILTER_BCJ2:   return "BCJ2";
        case SEVENZIP_FILTER_ARM:    return "ARM";
        case SEVENZIP_FILTER_ARM64:  return "ARM64";
        case SEVENZIP_FILTER_ARMT:   return "ARMT";
        case SEVENZIP_FILTER_IA64:   return "IA64";
        case SEVENZIP_FILTER_PPC:    return "PPC";
        case SEVENZIP_FILTER_SPARC:  return "SPARC";
        case SEVENZIP_FILTER_DELTA:  return "DELTA";
        default:                     return "Unknown";
    }
}
