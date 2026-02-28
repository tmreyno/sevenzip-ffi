#include "7z_ffi.h"
#include "7zFile.h"
#include "7zVersion.h"
#include "LzmaEnc.h"
#include "Lzma2Enc.h"
#include "Alloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

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

/* Helper: Check if path is a directory */
static int is_directory(const char* path) {
    struct STAT st;
    if (STAT(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

/* Helper: Check if path is a regular file */
static int is_regular_file(const char* path) {
    struct STAT st;
    if (STAT(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode);
}

/* Helper: Get file size */
static size_t get_file_size(const char* path) {
    struct STAT st;
    if (STAT(path, &st) != 0) {
        return 0;
    }
    return (size_t)st.st_size;
}

/* Helper: Read entire file into memory */
static unsigned char* read_file_contents(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(file);
        return NULL;
    }
    
    /* Allocate buffer */
    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    /* Read file */
    size_t read_bytes = fread(buffer, 1, size, file);
    fclose(file);
    
    if (read_bytes != (size_t)size) {
        free(buffer);
        return NULL;
    }
    
    *out_size = size;
    return buffer;
}

/* Helper: Convert compression level to LZMA2 properties */
static void get_lzma2_props_for_level(CLzma2EncProps* props, SevenZipCompressionLevel level) {
    Lzma2EncProps_Init(props);
    
    switch (level) {
        case SEVENZIP_LEVEL_STORE:
            /* Minimal compression */
            props->lzmaProps.level = 0;
            props->lzmaProps.dictSize = 1 << 16; /* 64 KB */
            break;
        case SEVENZIP_LEVEL_FASTEST:
            props->lzmaProps.level = 1;
            props->lzmaProps.dictSize = 1 << 18; /* 256 KB */
            break;
        case SEVENZIP_LEVEL_FAST:
            props->lzmaProps.level = 3;
            props->lzmaProps.dictSize = 1 << 20; /* 1 MB */
            break;
        case SEVENZIP_LEVEL_NORMAL:
            props->lzmaProps.level = 5;
            props->lzmaProps.dictSize = 1 << 23; /* 8 MB */
            props->numBlockThreads_Max = 1; /* Enable multi-threading for normal+ */
            break;
        case SEVENZIP_LEVEL_MAXIMUM:
            props->lzmaProps.level = 7;
            props->lzmaProps.dictSize = 1 << 25; /* 32 MB */
            props->numBlockThreads_Max = 2;
            break;
        case SEVENZIP_LEVEL_ULTRA:
            props->lzmaProps.level = 9;
            props->lzmaProps.dictSize = 1 << 26; /* 64 MB */
            props->numBlockThreads_Max = 2;
            break;
        default:
            props->lzmaProps.level = 5;
            props->lzmaProps.dictSize = 1 << 23;
            props->numBlockThreads_Max = 1;
            break;
    }
    
    Lzma2EncProps_Normalize(props);
}

/* Simplified compression for single files using LZMA2 format */
static SevenZipErrorCode compress_single_file_lzma2(
    const char* input_path,
    const char* output_path,
    SevenZipCompressionLevel level,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    /* Read input file */
    size_t input_size = 0;
    unsigned char* input_data = read_file_contents(input_path, &input_size);
    if (!input_data) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Create LZMA2 encoder */
    CLzma2EncHandle encoder = Lzma2Enc_Create(&g_Alloc, &g_Alloc);
    if (!encoder) {
        free(input_data);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Set compression properties */
    CLzma2EncProps props;
    get_lzma2_props_for_level(&props, level);
    
    SRes res = Lzma2Enc_SetProps(encoder, &props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(encoder);
        free(input_data);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    /* Get properties byte before encoding */
    Byte prop = Lzma2Enc_WriteProperties(encoder);
    
    /* Allocate output buffer (worst case: input size + overhead) */
    size_t output_buf_size = input_size + input_size / 3 + 128;
    unsigned char* output_data = (unsigned char*)malloc(output_buf_size);
    if (!output_data) {
        Lzma2Enc_Destroy(encoder);
        free(input_data);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Encode data */
    size_t output_size = output_buf_size;
    res = Lzma2Enc_Encode2(
        encoder,
        NULL,                  /* outStream - NULL to use buffer */
        output_data, &output_size,
        NULL,                  /* inStream - NULL to use buffer */
        input_data, input_size,
        NULL                   /* progress */
    );
    
    Lzma2Enc_Destroy(encoder);
    free(input_data);
    
    if (res != SZ_OK) {
        free(output_data);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    /* Write output file with LZMA2 header */
    FILE* out_file = fopen(output_path, "wb");
    if (!out_file) {
        free(output_data);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Write LZMA2 properties (1 byte) */
    fwrite(&prop, 1, 1, out_file);
    
    /* Write compressed data */
    fwrite(output_data, 1, output_size, out_file);
    fclose(out_file);
    free(output_data);
    
    /* Progress callback */
    if (progress_callback) {
        progress_callback(1, 1, user_data);
    }
    
    return SEVENZIP_OK;
}

SevenZipErrorCode sevenzip_compress(
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
    
    /* For now, handle single file compression to LZMA2 format
     * Full 7z archive creation requires more complex implementation
     * TODO: Implement full 7z format with multiple files/directories
     */
    
    if (num_inputs == 1 && is_regular_file(input_paths[0])) {
        /* Single file - use LZMA2 format */
        return compress_single_file_lzma2(
            input_paths[0],
            archive_path,
            level,
            progress_callback,
            user_data
        );
    }
    
    /* Multiple files or directories not yet supported
     * TODO: Implement using 7z format encoder:
     * 1. Traverse directories recursively
     * 2. Build file list with metadata
     * 3. Create 7z archive header
     * 4. Compress files with LZMA2
     * 5. Write solid or non-solid blocks
     * 6. Update archive header with offsets
     */
    
    return SEVENZIP_ERROR_NOT_IMPLEMENTED;
}

/* ============================================================================
 * LZMA1 Compression - sevenzip_compress_lzma
 * Output format: 5 bytes LZMA props + 8 bytes uncompressed size (LE) + data
 * This matches the .lzma format that sevenzip_decompress_lzma reads.
 * ============================================================================ */

SevenZipErrorCode sevenzip_compress_lzma(
    const char* input_path,
    const char* output_path,
    SevenZipCompressionLevel level,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!input_path || !output_path) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }

    if (!is_regular_file(input_path)) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }

    /* Read input file */
    size_t input_size = 0;
    unsigned char* input_data = read_file_contents(input_path, &input_size);
    if (!input_data) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }

    /* Set up LZMA1 encoder properties based on compression level */
    CLzmaEncProps props;
    LzmaEncProps_Init(&props);

    switch (level) {
        case SEVENZIP_LEVEL_STORE:
            props.level = 0;
            props.dictSize = 1 << 16; /* 64 KB */
            break;
        case SEVENZIP_LEVEL_FASTEST:
            props.level = 1;
            props.dictSize = 1 << 18; /* 256 KB */
            break;
        case SEVENZIP_LEVEL_FAST:
            props.level = 3;
            props.dictSize = 1 << 20; /* 1 MB */
            break;
        case SEVENZIP_LEVEL_NORMAL:
            props.level = 5;
            props.dictSize = 1 << 23; /* 8 MB */
            break;
        case SEVENZIP_LEVEL_MAXIMUM:
            props.level = 7;
            props.dictSize = 1 << 25; /* 32 MB */
            break;
        case SEVENZIP_LEVEL_ULTRA:
            props.level = 9;
            props.dictSize = 1 << 26; /* 64 MB */
            break;
        default:
            props.level = 5;
            props.dictSize = 1 << 23;
            break;
    }

    /* Allocate output buffer (worst case: input size + overhead) */
    SizeT dest_len = input_size + input_size / 3 + 128;
    unsigned char* dest = (unsigned char*)malloc(dest_len);
    if (!dest) {
        free(input_data);
        return SEVENZIP_ERROR_MEMORY;
    }

    /* Encode using one-call interface */
    Byte props_encoded[5];
    SizeT props_size = 5;

    SRes res = LzmaEncode(
        dest, &dest_len,
        input_data, input_size,
        &props, props_encoded, &props_size,
        0, /* writeEndMark = 0 (we store uncompressed size in header) */
        NULL, /* progress */
        &g_Alloc, &g_BigAlloc
    );

    free(input_data);

    if (res != SZ_OK) {
        free(dest);
        return SEVENZIP_ERROR_COMPRESS;
    }

    /* Write .lzma file: 5 bytes props + 8 bytes uncompressed size (LE) + data */
    FILE* out_file = fopen(output_path, "wb");
    if (!out_file) {
        free(dest);
        return SEVENZIP_ERROR_OPEN_FILE;
    }

    /* Write 5-byte LZMA properties */
    if (fwrite(props_encoded, 1, 5, out_file) != 5) {
        fclose(out_file);
        free(dest);
        remove(output_path);
        return SEVENZIP_ERROR_EXTRACT;
    }

    /* Write 8-byte uncompressed size (little-endian) */
    UInt64 unpack_size = (UInt64)input_size;
    Byte size_bytes[8];
    for (int i = 0; i < 8; i++) {
        size_bytes[i] = (Byte)(unpack_size >> (i * 8));
    }
    if (fwrite(size_bytes, 1, 8, out_file) != 8) {
        fclose(out_file);
        free(dest);
        remove(output_path);
        return SEVENZIP_ERROR_EXTRACT;
    }

    /* Write compressed data */
    if (fwrite(dest, 1, dest_len, out_file) != dest_len) {
        fclose(out_file);
        free(dest);
        remove(output_path);
        return SEVENZIP_ERROR_EXTRACT;
    }

    fclose(out_file);
    free(dest);

    /* Progress callback */
    if (progress_callback) {
        progress_callback(1, 1, user_data);
    }

    return SEVENZIP_OK;
}

/* ============================================================================
 * LZMA2 Compression - sevenzip_compress_lzma2
 * Output format: 1 byte LZMA2 property + LZMA2 compressed data
 * This matches the format that sevenzip_decompress_lzma2 reads.
 * ============================================================================ */

SevenZipErrorCode sevenzip_compress_lzma2(
    const char* input_path,
    const char* output_path,
    SevenZipCompressionLevel level,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!input_path || !output_path) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }

    if (!is_regular_file(input_path)) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }

    return compress_single_file_lzma2(
        input_path,
        output_path,
        level,
        progress_callback,
        user_data
    );
}
