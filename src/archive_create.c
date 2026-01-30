/**
 * 7z Archive Creation using LZMA2
 * 
 * Implements .7z format specification for creating archives compatible with 7-Zip
 */

#include "../include/7z_ffi.h"
#include "Lzma2Enc.h"
#include "7zCrc.h"
#include "Alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define STAT _stat
    #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#else
    #include <unistd.h>
    #define STAT stat
#endif

#define k7zSignature_Size 6
#define k7zMajorVersion 0
#define k7zStartHeaderSize 0x20

/* 7z format constants */
static const Byte k7zSignature[k7zSignature_Size] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

/* Property IDs for 7z headers */
typedef enum {
    k7zIdEnd = 0x00,
    k7zIdHeader = 0x01,
    k7zIdArchiveProperties = 0x02,
    k7zIdMainStreamsInfo = 0x04,
    k7zIdFilesInfo = 0x05,
    k7zIdPackInfo = 0x06,
    k7zIdUnpackInfo = 0x07,
    k7zIdSubStreamsInfo = 0x08,
    k7zIdSize = 0x09,
    k7zIdCRC = 0x0A,
    k7zIdFolder = 0x0B,
    k7zIdCodersUnpackSize = 0x0C,
    k7zIdNumUnpackStream = 0x0D,
    k7zIdEmptyStream = 0x0E,
    k7zIdEmptyFile = 0x0F,
    k7zIdAnti = 0x10,
    k7zIdName = 0x11,
    k7zIdCTime = 0x12,
    k7zIdATime = 0x13,
    k7zIdMTime = 0x14,
    k7zIdWinAttrib = 0x15,
    k7zIdStartPos = 0x17,
    k7zIdDummy = 0x19
} E7zIdEnum;

/* Test if data is compressible by checking entropy
 * Returns 1 if data appears compressible, 0 if it looks like random/encrypted data
 */
static int is_data_compressible(const Byte* data, size_t size) {
    if (size < 1024) return 1;  /* Small data - just compress it */
    
    /* Sample up to 64KB */
    size_t sample_size = (size < 65536) ? size : 65536;
    
    /* Count byte frequencies */
    size_t freq[256] = {0};
    for (size_t i = 0; i < sample_size; i++) {
        freq[data[i]]++;
    }
    
    /* Calculate entropy estimate - count unique bytes with significant frequency */
    int unique_bytes = 0;
    size_t threshold = sample_size / 512;  /* 0.2% threshold = 128 for 64KB sample */
    for (int i = 0; i < 256; i++) {
        if (freq[i] > threshold) unique_bytes++;
    }
    
    /* If >220 unique bytes appear frequently, data is likely incompressible (random/encrypted) */
    /* Typical text has ~60-100 unique frequent bytes */
    return unique_bytes < 220;
}

/* Coder method IDs */
static const Byte k7zMethodLZMA2[1] = { 0x21 };
static const Byte k7zMethodCopy[1] = { 0x00 };  /* Copy codec - no compression */

/* File entry structure */
typedef struct {
    char* name;
    uint64_t size;
    uint64_t pack_size;  /* Support files >4GB */
    uint64_t mtime;
    uint32_t attrib;
    uint32_t crc;
    Byte* data;  /* Raw data (for in-memory compression) */
    int is_dir;
} SevenZFile;

/* Archive builder */
typedef struct {
    SevenZFile* files;
    size_t file_count;
    size_t file_capacity;
    CLzma2EncProps props;
    Byte lzma2_prop_byte;  /* LZMA2 property byte for header */
    int use_copy_codec;    /* 1 = use Copy codec (store), 0 = use LZMA2 */
} SevenZArchiveBuilder;

/* Helper: Write number in variable-length encoding (7z format) 
 * 
 * 7z number encoding (from 7zArcIn.c ReadNumber):
 * - 0x00-0x7F: 1 byte, value = byte (7 bits)
 * - 0x80-0xBF: 2 bytes, value = ((first & 0x3F) << 8) | second (14 bits)
 * - 0xC0-0xDF: 3 bytes, value = byte2 | (byte3 << 8) | ((first & 0x1F) << 16) (21 bits)
 * - 0xE0-0xEF: 4 bytes, value = byte2 | (byte3 << 8) | (byte4 << 16) | ((first & 0x0F) << 24) (28 bits)
 * - 0xF0-0xF7: 5 bytes (35 bits)
 * - 0xF8-0xFB: 6 bytes (42 bits)
 * - 0xFC-0xFD: 7 bytes (49 bits)
 * - 0xFE:      8 bytes (56 bits)
 * - 0xFF:      9 bytes (64 bits)
 */
static void WriteNumber(Byte** buf, uint64_t value) {
    Byte* p = *buf;
    
    if (value < 0x80) {
        /* 1 byte: 7 bits */
        *p++ = (Byte)value;
    }
    else if (value < ((uint64_t)1 << 14)) {
        /* 2 bytes: 6 bits in first byte + 8 bits in second = 14 bits */
        *p++ = (Byte)(0x80 | (value >> 8));
        *p++ = (Byte)(value & 0xFF);
    }
    else if (value < ((uint64_t)1 << 21)) {
        /* 3 bytes: 5 bits in first byte + 16 bits little-endian = 21 bits */
        *p++ = (Byte)(0xC0 | ((value >> 16) & 0x1F));
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
    }
    else if (value < ((uint64_t)1 << 28)) {
        /* 4 bytes: 4 bits in first byte + 24 bits little-endian = 28 bits */
        *p++ = (Byte)(0xE0 | ((value >> 24) & 0x0F));
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
    }
    else if (value < ((uint64_t)1 << 35)) {
        /* 5 bytes: 3 bits in first byte + 32 bits little-endian = 35 bits */
        *p++ = (Byte)(0xF0 | ((value >> 32) & 0x07));
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
        *p++ = (Byte)((value >> 24) & 0xFF);
    }
    else if (value < ((uint64_t)1 << 42)) {
        /* 6 bytes: 2 bits in first byte + 40 bits little-endian = 42 bits */
        *p++ = (Byte)(0xF8 | ((value >> 40) & 0x03));
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
        *p++ = (Byte)((value >> 24) & 0xFF);
        *p++ = (Byte)((value >> 32) & 0xFF);
    }
    else if (value < ((uint64_t)1 << 49)) {
        /* 7 bytes: 1 bit in first byte + 48 bits little-endian = 49 bits */
        *p++ = (Byte)(0xFC | ((value >> 48) & 0x01));
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
        *p++ = (Byte)((value >> 24) & 0xFF);
        *p++ = (Byte)((value >> 32) & 0xFF);
        *p++ = (Byte)((value >> 40) & 0xFF);
    }
    else if (value < ((uint64_t)1 << 56)) {
        /* 8 bytes: first byte 0xFE + 56 bits little-endian */
        *p++ = 0xFE;
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
        *p++ = (Byte)((value >> 24) & 0xFF);
        *p++ = (Byte)((value >> 32) & 0xFF);
        *p++ = (Byte)((value >> 40) & 0xFF);
        *p++ = (Byte)((value >> 48) & 0xFF);
    }
    else {
        /* 9 bytes: first byte 0xFF + 64 bits little-endian */
        *p++ = 0xFF;
        *p++ = (Byte)(value & 0xFF);
        *p++ = (Byte)((value >> 8) & 0xFF);
        *p++ = (Byte)((value >> 16) & 0xFF);
        *p++ = (Byte)((value >> 24) & 0xFF);
        *p++ = (Byte)((value >> 32) & 0xFF);
        *p++ = (Byte)((value >> 40) & 0xFF);
        *p++ = (Byte)((value >> 48) & 0xFF);
        *p++ = (Byte)((value >> 56) & 0xFF);
    }
    
    *buf = p;
}

/* Helper: Calculate encoded number size */
static size_t GetNumberSize(uint64_t value) {
    int i;
    for (i = 0; i < 8; i++) {
        if (value < ((uint64_t)1 << (7 * (i + 1))))
            return i + 1;
    }
    return 9;
}

/* Helper: Add directory and its contents recursively */
#ifdef _WIN32
#include <windows.h>
static SevenZipErrorCode add_directory_recursive(
    SevenZArchiveBuilder* builder,
    const char* dir_path,
    const char* base_path
) {
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir_path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
        
        /* Expand array if needed */
        if (builder->file_count >= builder->file_capacity) {
            builder->file_capacity *= 2;
            SevenZFile* new_files = (SevenZFile*)realloc(
                builder->files, builder->file_capacity * sizeof(SevenZFile));
            if (!new_files) {
                FindClose(hFind);
                return SEVENZIP_ERROR_MEMORY;
            }
            builder->files = new_files;
        }
        
        SevenZFile* file = &builder->files[builder->file_count++];
        memset(file, 0, sizeof(SevenZFile));
        
        /* Compute relative path */
        const char* rel_path = full_path + strlen(base_path);
        while (*rel_path == '/' || *rel_path == '\\') rel_path++;
        file->name = strdup(rel_path);
        
        FILETIME ft = find_data.ftLastWriteTime;
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        file->mtime = ull.QuadPart;
        file->attrib = find_data.dwFileAttributes;
        file->is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        if (file->is_dir) {
            /* Recursively add directory contents */
            SevenZipErrorCode err = add_directory_recursive(builder, full_path, base_path);
            if (err != SEVENZIP_OK) {
                FindClose(hFind);
                return err;
            }
        } else {
            /* Read file data */
            FILE* f = fopen(full_path, "rb");
            if (!f) {
                FindClose(hFind);
                return SEVENZIP_ERROR_OPEN_FILE;
            }
            
            /* Use 1MB read buffer for optimal I/O performance */
            setvbuf(f, NULL, _IOFBF, 1024 * 1024);
            
            fseek(f, 0, SEEK_END);
            file->size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            file->data = (Byte*)malloc(file->size);
            if (!file->data) {
                fclose(f);
                FindClose(hFind);
                return SEVENZIP_ERROR_MEMORY;
            }
            
            if (fread(file->data, 1, file->size, f) != file->size) {
                fclose(f);
                FindClose(hFind);
                return SEVENZIP_ERROR_OPEN_FILE;
            }
            fclose(f);
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
    return SEVENZIP_OK;
}
#else
#include <dirent.h>
static SevenZipErrorCode add_directory_recursive(
    SevenZArchiveBuilder* builder,
    const char* dir_path,
    const char* base_path
) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        /* Get file info */
        struct STAT st;
        if (STAT(full_path, &st) != 0) {
            closedir(dir);
            return SEVENZIP_ERROR_OPEN_FILE;
        }
        
        /* Expand array if needed */
        if (builder->file_count >= builder->file_capacity) {
            builder->file_capacity *= 2;
            SevenZFile* new_files = (SevenZFile*)realloc(
                builder->files, builder->file_capacity * sizeof(SevenZFile));
            if (!new_files) {
                closedir(dir);
                return SEVENZIP_ERROR_MEMORY;
            }
            builder->files = new_files;
        }
        
        SevenZFile* file = &builder->files[builder->file_count++];
        memset(file, 0, sizeof(SevenZFile));
        
        /* Compute relative path */
        const char* rel_path = full_path + strlen(base_path);
        while (*rel_path == '/') rel_path++;
        file->name = strdup(rel_path);
        file->mtime = (uint64_t)st.st_mtime * 10000000ULL + 116444736000000000ULL;
        file->attrib = (uint32_t)st.st_mode;
        file->is_dir = S_ISDIR(st.st_mode);
        
        if (file->is_dir) {
            /* Recursively add directory contents */
            SevenZipErrorCode err = add_directory_recursive(builder, full_path, base_path);
            if (err != SEVENZIP_OK) {
                closedir(dir);
                return err;
            }
        } else if (S_ISREG(st.st_mode)) {
            /* Read file data */
            FILE* f = fopen(full_path, "rb");
            if (!f) {
                closedir(dir);
                return SEVENZIP_ERROR_OPEN_FILE;
            }
            
            /* Use 1MB read buffer for optimal I/O performance */
            setvbuf(f, NULL, _IOFBF, 1024 * 1024);
            
            file->size = st.st_size;
            file->data = (Byte*)malloc(file->size);
            if (!file->data) {
                fclose(f);
                closedir(dir);
                return SEVENZIP_ERROR_MEMORY;
            }
            
            if (fread(file->data, 1, file->size, f) != file->size) {
                fclose(f);
                closedir(dir);
                return SEVENZIP_ERROR_OPEN_FILE;
            }
            fclose(f);
        }
    }
    
    closedir(dir);
    return SEVENZIP_OK;
}
#endif

/* Helper: Compress all files into a single LZMA2 stream */
static SevenZipErrorCode compress_all_files(
    SevenZArchiveBuilder* builder,
    Byte** output,
    size_t* output_size
) {
    /* Calculate total input size */
    size_t total_input_size = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        if (!builder->files[i].is_dir && builder->files[i].data) {
            total_input_size += builder->files[i].size;
        }
    }
    
    if (total_input_size == 0) {
        *output = NULL;
        *output_size = 0;
        return SEVENZIP_OK;
    }
    
    /* Concatenate all file data into single buffer */
    Byte* combined = (Byte*)malloc(total_input_size);
    if (!combined) return SEVENZIP_ERROR_MEMORY;
    
    size_t offset = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        if (!builder->files[i].is_dir && builder->files[i].data) {
            memcpy(combined + offset, builder->files[i].data, builder->files[i].size);
            /* Calculate individual CRC while we're at it */
            builder->files[i].crc = CrcCalc(builder->files[i].data, builder->files[i].size);
            offset += builder->files[i].size;
        }
    }
    
    /* ADAPTIVE COMPRESSION: Check if data is compressible */
    /* For large data (>1MB), if it looks like random/encrypted data, use Copy codec */
    /* Also use Copy codec if explicitly requested (Store mode) */
    if (builder->use_copy_codec || 
        (total_input_size > 1024 * 1024 && !is_data_compressible(combined, total_input_size))) {
        /* Use Copy codec - return raw data directly (fastest possible) */
        builder->use_copy_codec = 1;
        builder->lzma2_prop_byte = 0;  /* Not used for Copy codec */
        
        *output = combined;  /* Return concatenated raw data */
        *output_size = total_input_size;
        return SEVENZIP_OK;
    }
    
    builder->use_copy_codec = 0;
    
    /* Create LZMA2 encoder */
    CLzma2EncHandle enc = Lzma2Enc_Create(&g_Alloc, &g_Alloc);
    if (!enc) {
        free(combined);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    SRes res = Lzma2Enc_SetProps(enc, &builder->props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(enc);
        free(combined);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    /* Get LZMA2 property byte for header */
    builder->lzma2_prop_byte = Lzma2Enc_WriteProperties(enc);
    
    /* Allocate output buffer (compressed data) */
    size_t out_size = total_input_size + total_input_size / 3 + 128;
    Byte* out_buf = (Byte*)malloc(out_size);
    if (!out_buf) {
        Lzma2Enc_Destroy(enc);
        free(combined);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Compress all data into single stream */
    res = Lzma2Enc_Encode2(enc, NULL, out_buf, &out_size,
                           NULL, combined, total_input_size, NULL);
    
    Lzma2Enc_Destroy(enc);
    free(combined);
    
    if (res != SZ_OK) {
        free(out_buf);
        return SEVENZIP_ERROR_COMPRESS;
    }
    
    /* NOTE: Lzma2Enc_Encode2 updates out_size to the actual bytes written.
     * The encoder produces a valid LZMA2 stream with proper end marker.
     * We trust the encoder's reported size directly.
     * 
     * Previous code tried to parse LZMA2 chunks to find the end marker,
     * but this was buggy because 0x00 bytes in uncompressed data could
     * be misinterpreted as end markers when parsing logic had errors.
     */
    
    *output = out_buf;
    *output_size = out_size;
    return SEVENZIP_OK;
}

/* Helper: Write 7z archive with proper format structure */
static SevenZipErrorCode write_7z_archive(
    const char* archive_path,
    SevenZArchiveBuilder* builder
) {
    FILE* f = fopen(archive_path, "wb");
    if (!f) return SEVENZIP_ERROR_OPEN_FILE;
    
    /* Use 4MB write buffer for optimal I/O performance */
    setvbuf(f, NULL, _IOFBF, 4 * 1024 * 1024);
    
    /* === WRITE SIGNATURE HEADER === */
    fwrite(k7zSignature, 1, k7zSignature_Size, f);
    
    /* Version (0.4) */
    Byte ver[2] = {k7zMajorVersion, 4};
    fwrite(ver, 1, 2, f);
    
    /* Start header CRC placeholder */
    long start_crc_pos = ftell(f);
    uint32_t dummy_crc = 0;
    fwrite(&dummy_crc, 4, 1, f);
    
    /* Next header offset and size placeholders */
    long next_header_offset_pos = ftell(f);
    uint64_t dummy_offset = 0;
    uint64_t dummy_size = 0;
    fwrite(&dummy_offset, 8, 1, f);
    fwrite(&dummy_size, 8, 1, f);
    
    /* Next header CRC placeholder */
    fwrite(&dummy_crc, 4, 1, f);
    
    /* === WRITE PACKED DATA === */
    /* Compress all files into a single LZMA2 stream */
    Byte* pack_data = NULL;
    size_t pack_size = 0;
    SevenZipErrorCode compress_err = compress_all_files(builder, &pack_data, &pack_size);
    if (compress_err != SEVENZIP_OK) {
        fclose(f);
        return compress_err;
    }
    
    long pack_pos = ftell(f);
    
    /* Write the single compressed stream */
    if (pack_data && pack_size > 0) {
        fwrite(pack_data, 1, pack_size, f);
    }
    
    /* === BUILD HEADER IN MEMORY === */
    size_t header_capacity = 65536;  /* 64KB buffer */
    Byte* header = (Byte*)malloc(header_capacity);
    if (!header) {
        free(pack_data);
        fclose(f);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    Byte* p = header;
    Byte* header_start = p;
    
    /* Header marker */
    *p++ = k7zIdHeader;
    
    /* === MainStreamsInfo === */
    *p++ = k7zIdMainStreamsInfo;
    
    /* --- PackInfo --- */
    *p++ = k7zIdPackInfo;
    WriteNumber(&p, 0);  /* Pack position (offset from end of SignatureHeader) */
    WriteNumber(&p, 1);  /* Number of pack streams - always 1 (all files in single stream) */
    
    /* Pack size */
    *p++ = k7zIdSize;
    WriteNumber(&p, pack_size);  /* Size of the single compressed stream */

    
    *p++ = k7zIdEnd;  /* End PackInfo */
    
    /* --- UnpackInfo --- */
    *p++ = k7zIdUnpackInfo;
    
    /* Folder */
    *p++ = k7zIdFolder;
    WriteNumber(&p, 1);  /* Number of folders */
    
    /* External flag (0 = not external) */
    WriteNumber(&p, 0);
    
    /* Number of coders */
    WriteNumber(&p, 1);
    
    if (builder->use_copy_codec) {
        /* Coder flags byte for Copy codec:
         *   Bits 7-6: reserved (0)
         *   Bit 5: HasProperties (0 = no property data)
         *   Bit 4: IsComplex (0 = simple coder)
         *   Bits 0-3: Codec ID size (1 byte)
         * Value: 0x01 = 00000001 = ID_size=1, no properties
         */
        *p++ = 0x01;
        
        /* Codec ID (Copy = 0x00) */
        *p++ = 0x00;
        
        /* No property data for Copy codec */
    } else {
        /* Coder flags byte for LZMA2:
         *   Bits 7-6: reserved (0)
         *   Bit 5: HasProperties (1 = has property data after codec ID)
         *   Bit 4: IsComplex (0 = simple coder, no NumIn/NumOut)
         *   Bits 0-3: Codec ID size (1 byte for LZMA2)
         * Value: 0x21 = 00100001 = HasProperties + ID_size=1
         */
        *p++ = 0x21;
        
        /* Codec ID (LZMA2 = 0x21) */
        *p++ = 0x21;
        
        /* Property data (because HasProperties bit is set) */
        WriteNumber(&p, 1);  /* Properties size = 1 byte */
        *p++ = builder->lzma2_prop_byte;  /* Actual LZMA2 property byte */
    }
    
    /* CoderUnpackSizes */
    *p++ = k7zIdCodersUnpackSize;
    uint64_t total_unpack_size = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        if (!builder->files[i].is_dir) {
            total_unpack_size += builder->files[i].size;
        }
    }
    WriteNumber(&p, total_unpack_size);
    
    *p++ = k7zIdEnd;  /* End UnpackInfo */
    
    /* --- SubStreamsInfo --- */
    *p++ = k7zIdSubStreamsInfo;
    
    /* Count non-directory files */
    size_t num_files = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        if (!builder->files[i].is_dir) {
            num_files++;
        }
    }
    
    /* Number of unpack streams per folder */
    *p++ = k7zIdNumUnpackStream;
    WriteNumber(&p, num_files);  /* Number of files extracted from single stream */
    
    /* Individual file sizes (all but last - last is implied) */
    if (num_files > 1) {
        *p++ = k7zIdSize;
        size_t written = 0;
        for (size_t i = 0; i < builder->file_count && written < num_files - 1; i++) {
            if (!builder->files[i].is_dir) {
                WriteNumber(&p, builder->files[i].size);
                written++;
            }
        }
    }
    
    /* CRC values for all files */
    *p++ = k7zIdCRC;
    *p++ = 1;  /* All CRCs are defined */
    for (size_t i = 0; i < builder->file_count; i++) {
        if (!builder->files[i].is_dir) {
            /* Write CRC in little-endian */
            uint32_t crc_le = builder->files[i].crc;
            memcpy(p, &crc_le, 4);
            p += 4;
        }
    }

    
    *p++ = k7zIdEnd;  /* End SubStreamsInfo */
    *p++ = k7zIdEnd;  /* End MainStreamsInfo */
    
    /* === FilesInfo === */
    *p++ = k7zIdFilesInfo;
    WriteNumber(&p, builder->file_count);
    
    /* EmptyStream bit vector (for directories) */
    int has_dirs = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        if (builder->files[i].is_dir) {
            has_dirs = 1;
            break;
        }
    }
    
    if (has_dirs) {
        *p++ = k7zIdEmptyStream;
        size_t mask_size = (builder->file_count + 7) / 8;
        WriteNumber(&p, mask_size);
        memset(p, 0, mask_size);
        for (size_t i = 0; i < builder->file_count; i++) {
            if (builder->files[i].is_dir) {
                p[i / 8] |= (1 << (7 - (i % 8)));
            }
        }
        p += mask_size;
    }
    
    /* Names (UTF-16LE) */
    *p++ = k7zIdName;
    size_t names_size = 0;
    for (size_t i = 0; i < builder->file_count; i++) {
        names_size += (strlen(builder->files[i].name) + 1) * 2;
    }
    WriteNumber(&p, names_size + 1);
    *p++ = 0;  /* External flag = 0 (names embedded) */
    
    for (size_t i = 0; i < builder->file_count; i++) {
        const char* name = builder->files[i].name;
        while (*name) {
            *p++ = (Byte)*name++;
            *p++ = 0;  /* High byte of UTF-16LE */
        }
        *p++ = 0;  /* Null terminator low byte */
        *p++ = 0;  /* Null terminator high byte */
    }
    
    /* Modification times (Windows FILETIME format) */
    *p++ = k7zIdMTime;
    size_t time_size = 2 + (8 * builder->file_count);
    WriteNumber(&p, time_size);
    *p++ = 1;  /* All times defined */
    *p++ = 0;  /* External flag = 0 */
    for (size_t i = 0; i < builder->file_count; i++) {
        uint64_t filetime = builder->files[i].mtime;
        memcpy(p, &filetime, 8);
        p += 8;
    }
    
    /* Attributes */
    *p++ = k7zIdWinAttrib;
    size_t attrib_size = 2 + (4 * builder->file_count);
    WriteNumber(&p, attrib_size);
    *p++ = 1;  /* All attributes defined */
    *p++ = 0;  /* External flag = 0 */
    for (size_t i = 0; i < builder->file_count; i++) {
        uint32_t attr = builder->files[i].attrib;
        memcpy(p, &attr, 4);
        p += 4;
    }
    
    *p++ = k7zIdEnd;  /* End FilesInfo */
    *p++ = k7zIdEnd;  /* End Header */
    
    /* === FINALIZE HEADER === */
    size_t actual_header_size = p - header_start;
    
    /* Ensure we didn't overflow */
    if (actual_header_size > header_capacity) {
        free(header);
        fclose(f);
        return SEVENZIP_ERROR_COMPRESS;  /* Header too large */
    }
    
    /* Calculate header CRC */
    uint32_t header_crc = CrcCalc(header_start, actual_header_size);
    
    /* Write header to file */
    uint64_t header_offset = ftell(f) - pack_pos;
    fwrite(header_start, 1, actual_header_size, f);
    free(header);
    
    /* === UPDATE START HEADER === */
    /* Go back and write the actual values */
    fseek(f, next_header_offset_pos, SEEK_SET);
    fwrite(&header_offset, 8, 1, f);
    uint64_t header_size_field = actual_header_size;
    fwrite(&header_size_field, 8, 1, f);
    fwrite(&header_crc, 4, 1, f);
    
    /* Calculate and write start header CRC */
    Byte start_header[20];
    memcpy(start_header, &header_offset, 8);
    memcpy(start_header + 8, &header_size_field, 8);
    memcpy(start_header + 16, &header_crc, 4);
    uint32_t start_header_crc = CrcCalc(start_header, 20);
    
    fseek(f, start_crc_pos, SEEK_SET);
    fwrite(&start_header_crc, 4, 1, f);
    
    fclose(f);
    free(pack_data);  /* Free compressed data */
    return SEVENZIP_OK;
}

/* Main API: Create 7z archive */
SevenZipErrorCode sevenzip_create_7z(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const SevenZipCompressOptions* options,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !input_paths) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Initialize CRC tables */
    CrcGenerateTable();
    
    /* Set default options if not provided */
    SevenZipCompressOptions default_opts = {
        .num_threads = 2,
        .dict_size = 0,  /* Auto */
        .solid = 1,       /* Solid archive */
        .password = NULL  /* No encryption */
    };
    const SevenZipCompressOptions* opts = options ? options : &default_opts;
    
    /* Create builder */
    SevenZArchiveBuilder builder;
    builder.file_count = 0;
    builder.file_capacity = 16;
    builder.use_copy_codec = 0;  /* Default: use LZMA2 compression */
    builder.files = (SevenZFile*)calloc(builder.file_capacity, sizeof(SevenZFile));
    if (!builder.files) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Set compression properties */
    Lzma2EncProps_Init(&builder.props);
    
    /* Apply thread count from options - OPTIMIZED for block-level parallelism
     * LZMA2 achieves parallelism by compressing multiple blocks simultaneously.
     * Formula: numTotalThreads = numBlockThreads × lzmaThreads
     * For max throughput: use N/2 block threads with 2 LZMA threads each
     */
    if (opts->num_threads > 0) {
        int block_threads = opts->num_threads / 2;
        if (block_threads < 1) block_threads = 1;
        builder.props.numBlockThreads_Max = block_threads;
        builder.props.lzmaProps.numThreads = 2;  /* 2 threads per block encoder */
        builder.props.numTotalThreads = opts->num_threads;
        /* Set explicit block size for parallel compression (4x dictionary) */
        builder.props.blockSize = 0;  /* 0 = auto-calculate based on dict size */
    }
    
    switch (level) {
        case SEVENZIP_LEVEL_STORE:
            builder.use_copy_codec = 1;  /* Use Copy codec for Store mode */
            builder.props.lzmaProps.level = 0;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 16);
            break;
        case SEVENZIP_LEVEL_FASTEST:
            builder.props.lzmaProps.level = 1;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 18);
            break;
        case SEVENZIP_LEVEL_FAST:
            builder.props.lzmaProps.level = 3;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 20);
            break;
        case SEVENZIP_LEVEL_NORMAL:
            builder.props.lzmaProps.level = 5;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 23);
            if (opts->num_threads == 0) builder.props.numBlockThreads_Max = 2;
            break;
        case SEVENZIP_LEVEL_MAXIMUM:
            builder.props.lzmaProps.level = 7;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 25);
            if (opts->num_threads == 0) builder.props.numBlockThreads_Max = 2;
            break;
        case SEVENZIP_LEVEL_ULTRA:
            builder.props.lzmaProps.level = 9;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 26);
            if (opts->num_threads == 0) builder.props.numBlockThreads_Max = 2;
            break;
        default:
            builder.props.lzmaProps.level = 5;
            builder.props.lzmaProps.dictSize = opts->dict_size > 0 ? opts->dict_size : (1 << 23);
    }
    Lzma2EncProps_Normalize(&builder.props);
    
    /* Count files */
    size_t total_files = 0;
    for (const char** p = input_paths; *p; p++) total_files++;
    
    /* Add files/directories */
    SevenZipErrorCode result = SEVENZIP_OK;
    for (size_t i = 0; i < total_files; i++) {
        const char* path = input_paths[i];
        
        /* Get file info */
        struct STAT st;
        if (STAT(path, &st) != 0) {
            result = SEVENZIP_ERROR_OPEN_FILE;
            goto cleanup;
        }
        
        if (S_ISDIR(st.st_mode)) {
            /* Add directory recursively */
            result = add_directory_recursive(&builder, path, path);
            if (result != SEVENZIP_OK) {
                goto cleanup;
            }
        } else {
            /* Expand array if needed */
            if (builder.file_count >= builder.file_capacity) {
                builder.file_capacity *= 2;
                SevenZFile* new_files = (SevenZFile*)realloc(
                    builder.files, builder.file_capacity * sizeof(SevenZFile));
                if (!new_files) {
                    result = SEVENZIP_ERROR_MEMORY;
                    goto cleanup;
                }
                builder.files = new_files;
            }
            
            SevenZFile* file = &builder.files[builder.file_count++];
            memset(file, 0, sizeof(SevenZFile));
            
            /* Extract filename */
            const char* name = strrchr(path, '/');
            if (!name) name = strrchr(path, '\\');
            file->name = strdup(name ? name + 1 : path);
            file->mtime = (uint64_t)st.st_mtime * 10000000ULL + 116444736000000000ULL;
            file->attrib = (uint32_t)st.st_mode;
            file->is_dir = 0;  /* Regular file */
            
            if (S_ISREG(st.st_mode)) {
                /* Read file */
                FILE* f = fopen(path, "rb");
                if (!f) {
                    result = SEVENZIP_ERROR_OPEN_FILE;
                    goto cleanup;
                }
                
                file->size = st.st_size;
                Byte* file_data = (Byte*)malloc(file->size);
                if (!file_data) {
                    fclose(f);
                    result = SEVENZIP_ERROR_MEMORY;
                    goto cleanup;
                }
                
                if (fread(file_data, 1, file->size, f) != file->size) {
                    free(file_data);
                    fclose(f);
                    result = SEVENZIP_ERROR_OPEN_FILE;
                    goto cleanup;
                }
                fclose(f);
                
                /* Store raw data (compression happens later for all files together) */
                file->data = file_data;
                file->pack_size = file->size;  /* Will be updated after compression */
                file->crc = 0;  /* Will be calculated during compression */
            }
        }
        
        /* Progress callback */
        if (progress_callback) {
            progress_callback(i + 1, total_files, user_data);
        }
    }
    
    /* Write archive */
    result = write_7z_archive(archive_path, &builder);
    
cleanup:
    /* Free resources */
    for (size_t i = 0; i < builder.file_count; i++) {
        if (builder.files[i].name) free(builder.files[i].name);
        if (builder.files[i].data) free(builder.files[i].data);
    }
    free(builder.files);
    
    return result;
}
