/**
 * Complete 7z Multi-Volume Archive Implementation
 * 
 * Implements full 7z format specification for split archives.
 * Supports files 100GB+ by streaming across multiple volumes.
 * 
 * Format: archive.7z.001, archive.7z.002, etc.
 * Compatible with 7-Zip for extraction.
 */

#include "../include/7z_ffi.h"
#include "../lzma/C/7zFile.h"
#include "../lzma/C/7zTypes.h"
#include "../lzma/C/7zCrc.h"
#include "../lzma/C/Lzma2Enc.h"
#include "../lzma/C/Alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

#ifdef _WIN32
    #include <windows.h>
    #define STAT _stat
    #define PATH_SEP '\\'
    #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #define USE_MMAP 0
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/mman.h>
    #include <fcntl.h>
    #define STAT stat
    #define PATH_SEP '/'
    #define USE_MMAP 1
#endif

/* 7z format constants */
#define k7zSignature_Size 6
#define k7zStartHeaderSize 32

static const Byte k7zSignature[k7zSignature_Size] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

/* Property IDs */
typedef enum {
    k7zIdEnd = 0x00,
    k7zIdHeader = 0x01,
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
    k7zIdName = 0x11,
    k7zIdMTime = 0x14,
    k7zIdWinAttrib = 0x15
} E7zIdEnum;

/* File entry for multi-volume archives */
typedef struct {
    char* name;
    char* full_path;  /* Full path for reading the file */
    uint64_t size;
    uint64_t mtime;
    uint32_t attrib;
    uint32_t crc;
    Byte lzma2_prop;  /* LZMA2 property byte for this file */
    int is_dir;
} MV_FileEntry;

/* File list for gathering entries */
typedef struct {
    MV_FileEntry* entries;
    size_t count;
    size_t capacity;
    uint64_t total_size;
} MV_FileList;

/* Initialize file list */
static void mv_file_list_init(MV_FileList* list) {
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
    list->total_size = 0;
}

/* Free file list */
static void mv_file_list_free(MV_FileList* list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->entries[i].name);
        free(list->entries[i].full_path);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Add file to list */
static int mv_file_list_add(MV_FileList* list, const char* full_path, const char* archive_name, uint64_t size, uint64_t mtime, uint32_t attrib) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 64 : list->capacity * 2;
        MV_FileEntry* new_entries = (MV_FileEntry*)realloc(list->entries, new_cap * sizeof(MV_FileEntry));
        if (!new_entries) return 0;
        list->entries = new_entries;
        list->capacity = new_cap;
    }
    
    MV_FileEntry* entry = &list->entries[list->count];
    memset(entry, 0, sizeof(MV_FileEntry));
    entry->full_path = strdup(full_path);
    entry->name = strdup(archive_name);
    entry->size = size;
    entry->mtime = mtime;
    entry->attrib = attrib;
    entry->is_dir = 0;
    
    list->count++;
    list->total_size += size;
    return 1;
}

/* Forward declaration for recursion */
static int mv_gather_directory(const char* dir_path, const char* base_name, MV_FileList* list);

/* Gather files from a path (file or directory) */
static int mv_gather_files(const char* path, const char* base_name, MV_FileList* list) {
    struct STAT st;
    if (STAT(path, &st) != 0) {
        fprintf(stderr, "mv_gather: Cannot stat: %s\n", path);
        return 0;
    }
    
    if (S_ISREG(st.st_mode)) {
        /* Regular file - use base_name if provided, otherwise just filename */
        const char* name = base_name;
        if (!name) {
            name = strrchr(path, PATH_SEP);
            name = name ? name + 1 : path;
        }
        
        /* Convert Unix time to Windows FILETIME */
        uint64_t mtime = ((uint64_t)st.st_mtime * 10000000ULL) + 116444736000000000ULL;
        
        /* Set attributes */
        uint32_t attrib = 0x20;  /* FILE_ATTRIBUTE_ARCHIVE */
        if (!(st.st_mode & S_IWUSR)) {
            attrib |= 0x01;  /* FILE_ATTRIBUTE_READONLY */
        }
        
        return mv_file_list_add(list, path, name, st.st_size, mtime, attrib);
    } else if (S_ISDIR(st.st_mode)) {
        return mv_gather_directory(path, base_name, list);
    }
    
    return 1;  /* Skip other types */
}

/* Gather files from a directory recursively */
static int mv_gather_directory(const char* dir_path, const char* base_name, MV_FileList* list) {
#ifdef _WIN32
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);
        
        char archive_name[1024];
        if (base_name) {
            snprintf(archive_name, sizeof(archive_name), "%s/%s", base_name, fd.cFileName);
        } else {
            snprintf(archive_name, sizeof(archive_name), "%s", fd.cFileName);
        }
        
        if (!mv_gather_files(full_path, archive_name, list)) {
            FindClose(hFind);
            return 0;
        }
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "mv_gather: Cannot open directory: %s\n", dir_path);
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        char archive_name[1024];
        if (base_name) {
            snprintf(archive_name, sizeof(archive_name), "%s/%s", base_name, entry->d_name);
        } else {
            /* Use the directory name as the base */
            const char* dir_name = strrchr(dir_path, PATH_SEP);
            dir_name = dir_name ? dir_name + 1 : dir_path;
            snprintf(archive_name, sizeof(archive_name), "%s/%s", dir_name, entry->d_name);
        }
        
        if (!mv_gather_files(full_path, archive_name, list)) {
            closedir(dir);
            return 0;
        }
    }
    
    closedir(dir);
#endif
    return 1;
}

/* Multi-volume context */
typedef struct {
    FILE** volumes;
    size_t volume_count;
    size_t volume_capacity;
    uint64_t current_volume_size;
    uint64_t max_volume_size;
    char base_path[1024];
    
    /* Compressed data tracking */
    uint64_t total_packed_size;
    
    /* Progress */
    SevenZipBytesProgressCallback progress_callback;
    void* user_data;
    uint64_t total_size;
    uint64_t bytes_written;
} MultiVolumeContext;

/* Helper: Write number in 7z variable-length encoding (little-endian for bytes after first)
 * 
 * 7z uses a variable-length encoding where the first byte indicates how many additional
 * bytes follow:
 * - 0x00-0x7F:  1 byte  (7 bits of data)
 * - 0x80-0xBF:  2 bytes (6 bits in first + 8 bits in second = 14 bits)
 * - 0xC0-0xDF:  3 bytes (5 bits in first + 16 bits LE = 21 bits)
 * - 0xE0-0xEF:  4 bytes (4 bits in first + 24 bits LE = 28 bits)
 * - 0xF0-0xF7:  5 bytes (3 bits in first + 32 bits LE = 35 bits)
 * - 0xF8-0xFB:  6 bytes (2 bits in first + 40 bits LE = 42 bits)
 * - 0xFC-0xFD:  7 bytes (1 bit in first + 48 bits LE = 49 bits)
 * - 0xFE:       8 bytes (56 bits LE)
 * - 0xFF:       9 bytes (64 bits LE)
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

/* Helper: Get volume filename */
static void get_volume_filename(char* buffer, size_t size, const char* base, int index) {
    snprintf(buffer, size, "%s.%03d", base, index + 1);
}

/* Helper: Open new volume file */
static FILE* open_new_volume(MultiVolumeContext* ctx) {
    if (ctx->volume_count >= ctx->volume_capacity) {
        ctx->volume_capacity *= 2;
        FILE** new_vols = (FILE**)realloc(ctx->volumes, ctx->volume_capacity * sizeof(FILE*));
        if (!new_vols) return NULL;
        ctx->volumes = new_vols;
    }
    
    char vol_path[1280];
    get_volume_filename(vol_path, sizeof(vol_path), ctx->base_path, ctx->volume_count);
    
    FILE* f = fopen(vol_path, "wb");
    if (!f) return NULL;
    
    /* Use larger buffer for faster I/O (4MB for output) */
    setvbuf(f, NULL, _IOFBF, 4 * 1024 * 1024);
    
    ctx->volumes[ctx->volume_count++] = f;
    ctx->current_volume_size = 0;
    
    return f;
}

/* Helper: Write data across volumes */
static int write_across_volumes(MultiVolumeContext* ctx, const void* data, size_t size) {
    const Byte* src = (const Byte*)data;
    size_t remaining = size;
    
    while (remaining > 0) {
        FILE* current = (ctx->volume_count > 0) ? ctx->volumes[ctx->volume_count - 1] : NULL;
        
        /* Need new volume? */
        if (!current || ctx->current_volume_size >= ctx->max_volume_size) {
            current = open_new_volume(ctx);
            if (!current) return 0;
        }
        
        /* Calculate how much to write to current volume */
        size_t space_in_volume = ctx->max_volume_size - ctx->current_volume_size;
        size_t to_write = (remaining < space_in_volume) ? remaining : space_in_volume;
        
        if (fwrite(src, 1, to_write, current) != to_write) {
            return 0;
        }
        
        src += to_write;
        remaining -= to_write;
        ctx->current_volume_size += to_write;
        ctx->bytes_written += to_write;  /* Track all bytes written including header */
        
        /* Progress callback */
        if (ctx->progress_callback && ctx->total_size > 0) {
            ctx->progress_callback(
                ctx->bytes_written,
                ctx->total_size,
                to_write,
                0,
                "",
                ctx->user_data
            );
        }
    }
    
    return 1;
}

/* ============================================================================
 * Streaming interfaces for LZMA2 encoding
 * ============================================================================ */

/* Large read buffer size for optimal I/O throughput */
#define STREAM_BUFFER_SIZE (32 * 1024 * 1024)  /* 32MB buffer - better I/O batching */

#if USE_MMAP
/* Memory-mapped input stream - FASTEST possible I/O */
typedef struct {
    ISeqInStream vt;       /* Virtual table - MUST be first! */
    const Byte* data;      /* Memory-mapped data */
    uint64_t size;         /* Total file size */
    uint64_t pos;          /* Current position */
    uint32_t* crc;         /* Pointer to CRC accumulator */
    int fd;                /* File descriptor for cleanup */
} MmapInStream;

/* ISeqInStream::Read for mmap - zero-copy reads! */
static SRes MmapInStream_Read(const ISeqInStream *p, void *buf, size_t *size) {
    MmapInStream *s = (MmapInStream *)p;
    
    size_t remaining = s->size - s->pos;
    size_t to_read = (*size < remaining) ? *size : remaining;
    
    if (to_read == 0) {
        *size = 0;
        return SZ_OK;
    }
    
    /* Direct memory copy - no system calls! */
    memcpy(buf, s->data + s->pos, to_read);
    
    /* Update CRC */
    if (s->crc) {
        *s->crc = CrcUpdate(*s->crc, s->data + s->pos, to_read);
    }
    
    s->pos += to_read;
    *size = to_read;
    return SZ_OK;
}
#endif

/* Input stream context for reading from file while computing CRC */
typedef struct {
    ISeqInStream vt;       /* Virtual table - MUST be first! */
    FILE* file;
    uint64_t remaining;
    uint32_t* crc;         /* Pointer to CRC accumulator */
    /* Buffered reading for better I/O performance */
    Byte* buffer;
    size_t buf_size;
    size_t buf_pos;
} FileInStream;

/* Output stream context for writing to multi-volume archive */
typedef struct {
    ISeqOutStream vt;      /* Virtual table - MUST be first! */
    MultiVolumeContext* ctx;
    uint64_t* packed_size;
    /* Write buffer for coalescing small writes */
    Byte* buffer;
    size_t buf_size;
    size_t buf_pos;
} VolumeOutStream;

/* ISeqInStream::Read implementation - OPTIMIZED with buffering */
static SRes FileInStream_Read(const ISeqInStream *p, void *buf, size_t *size) {
    FileInStream *s = (FileInStream *)p;
    
    size_t requested = *size;
    if (requested > s->remaining) {
        requested = (size_t)s->remaining;
    }
    
    if (requested == 0) {
        *size = 0;
        return SZ_OK;
    }
    
    Byte* dst = (Byte*)buf;
    size_t total_read = 0;
    
    while (total_read < requested) {
        /* Refill buffer if empty */
        if (s->buf_pos >= s->buf_size) {
            size_t to_read = STREAM_BUFFER_SIZE;
            if (to_read > s->remaining - total_read) {
                to_read = (size_t)(s->remaining - total_read);
            }
            if (to_read == 0) break;
            
            s->buf_size = fread(s->buffer, 1, to_read, s->file);
            s->buf_pos = 0;
            
            if (s->buf_size == 0) break;
        }
        
        /* Copy from buffer */
        size_t avail = s->buf_size - s->buf_pos;
        size_t need = requested - total_read;
        size_t copy = (avail < need) ? avail : need;
        
        memcpy(dst + total_read, s->buffer + s->buf_pos, copy);
        
        /* Update CRC incrementally */
        if (s->crc) {
            *s->crc = CrcUpdate(*s->crc, s->buffer + s->buf_pos, copy);
        }
        
        s->buf_pos += copy;
        total_read += copy;
    }
    
    s->remaining -= total_read;
    *size = total_read;
    
    return (total_read == 0 && requested > 0) ? SZ_ERROR_READ : SZ_OK;
}

/* Flush output buffer */
static int VolumeOutStream_Flush(VolumeOutStream *s) {
    if (s->buf_pos > 0) {
        if (!write_across_volumes(s->ctx, s->buffer, s->buf_pos)) {
            return 0;
        }
        *s->packed_size += s->buf_pos;
        s->buf_pos = 0;
    }
    return 1;
}

/* ISeqOutStream::Write implementation - OPTIMIZED with buffering */
static size_t VolumeOutStream_Write(const ISeqOutStream *p, const void *buf, size_t size) {
    VolumeOutStream *s = (VolumeOutStream *)p;
    
    /* For large writes, bypass buffer and write directly */
    if (size >= STREAM_BUFFER_SIZE) {
        /* Flush existing buffer first */
        if (!VolumeOutStream_Flush(s)) return 0;
        
        if (!write_across_volumes(s->ctx, buf, size)) {
            return 0;
        }
        *s->packed_size += size;
        return size;
    }
    
    /* Buffer small writes */
    const Byte* src = (const Byte*)buf;
    size_t remaining = size;
    
    while (remaining > 0) {
        size_t space = STREAM_BUFFER_SIZE - s->buf_pos;
        size_t copy = (remaining < space) ? remaining : space;
        
        memcpy(s->buffer + s->buf_pos, src, copy);
        s->buf_pos += copy;
        src += copy;
        remaining -= copy;
        
        /* Flush if full */
        if (s->buf_pos >= STREAM_BUFFER_SIZE) {
            if (!VolumeOutStream_Flush(s)) return 0;
        }
    }
    
    return size;
}

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
    
    /* If >200 unique bytes appear frequently, data is likely incompressible (random/encrypted) */
    /* Typical text has ~60-100 unique frequent bytes */
    return unique_bytes < 200;
}

/* Store file without compression (fast copy) */
static SRes store_file_uncompressed(
    const char* file_path,
    const Byte* mapped_data,  /* Can be NULL */
    uint64_t file_size,
    MultiVolumeContext* ctx,
    uint32_t* out_crc,
    uint64_t* out_packed_size
) {
    uint32_t crc = CRC_INIT_VAL;
    
    if (mapped_data) {
        /* Fast path: data is already mapped */
        crc = CrcUpdate(crc, mapped_data, file_size);
        if (!write_across_volumes(ctx, mapped_data, file_size)) {
            return SZ_ERROR_WRITE;
        }
    } else {
        /* Fallback: read file in chunks */
        FILE* f = fopen(file_path, "rb");
        if (!f) {
            fprintf(stderr, "DEBUG: Cannot open file: %s\n", file_path);
            return SZ_ERROR_READ;
        }
        
        /* Use 64KB buffer for very large files to reduce memory pressure */
        size_t buf_size = (file_size > 4ULL * 1024 * 1024 * 1024) ? 
                          (64 * 1024) : STREAM_BUFFER_SIZE;
        Byte* buffer = (Byte*)malloc(buf_size);
        if (!buffer) {
            fclose(f);
            return SZ_ERROR_MEM;
        }
        
        uint64_t remaining = file_size;
        uint64_t total_read = 0;
        while (remaining > 0) {
            size_t to_read = (remaining < buf_size) ? (size_t)remaining : buf_size;
            size_t got = fread(buffer, 1, to_read, f);
            if (got == 0) {
                if (ferror(f)) {
                    fprintf(stderr, "DEBUG: Read error at offset %llu\n", total_read);
                    free(buffer);
                    fclose(f);
                    return SZ_ERROR_READ;
                }
                break;  /* EOF */
            }
            
            crc = CrcUpdate(crc, buffer, got);
            if (!write_across_volumes(ctx, buffer, got)) {
                fprintf(stderr, "DEBUG: Write error at offset %llu\n", total_read);
                free(buffer);
                fclose(f);
                return SZ_ERROR_WRITE;
            }
            remaining -= got;
            total_read += got;
        }
        free(buffer);
        fclose(f);
    }
    
    *out_crc = CRC_GET_DIGEST(crc);
    *out_packed_size = file_size;  /* No compression */
    return SZ_OK;
}

/* Compress file and write to volumes using proper streaming */
static SRes compress_file_streaming(
    const char* file_path,
    MultiVolumeContext* ctx,
    const CLzma2EncProps* props,
    uint32_t* out_crc,
    uint64_t* out_packed_size,
    Byte* out_prop
) {
    struct STAT st;
    if (STAT(file_path, &st) != 0) return SZ_ERROR_READ;
    uint64_t file_size = st.st_size;
    
#if USE_MMAP
    /* Try memory-mapped I/O for maximum speed */
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) return SZ_ERROR_READ;
    
    /* Use mmap for files - enables zero-copy reads */
    void* mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        /* Fall back to FILE* based reading */
        goto fallback_read;
    }
    
    /* Advise kernel we'll read sequentially */
    madvise(mapped, file_size, MADV_SEQUENTIAL | MADV_WILLNEED);
    
    /* ADAPTIVE: Check if data is compressible */
    /* For large files (>1MB), if data looks random, skip compression */
    int compressible = is_data_compressible((const Byte*)mapped, file_size);
    if (file_size > 1024 * 1024 && !compressible) {
        /* Data appears incompressible - use store mode for MASSIVE speed gain */
        *out_prop = 0;  /* Store indicator */
        SRes res = store_file_uncompressed(file_path, (const Byte*)mapped, file_size, ctx, out_crc, out_packed_size);
        munmap(mapped, file_size);
        close(fd);
        return res;
    }
    
    /* Create encoder */
    CLzma2EncHandle enc = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
    if (!enc) {
        munmap(mapped, file_size);
        close(fd);
        return SZ_ERROR_MEM;
    }
    
    /* Set expected data size - helps encoder optimize block threading */
    Lzma2Enc_SetDataSize(enc, file_size);
    
    SRes res = Lzma2Enc_SetProps(enc, props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(enc);
        munmap(mapped, file_size);
        close(fd);
        return res;
    }
    
    Byte prop = Lzma2Enc_WriteProperties(enc);
    *out_prop = prop;
    
    uint32_t crc = CRC_INIT_VAL;
    uint64_t packed_size = 0;
    
    /* Allocate output buffer */
    Byte* out_buffer = (Byte*)malloc(STREAM_BUFFER_SIZE);
    if (!out_buffer) {
        Lzma2Enc_Destroy(enc);
        munmap(mapped, file_size);
        close(fd);
        return SZ_ERROR_MEM;
    }
    
    /* Setup mmap input stream - zero-copy! */
    MmapInStream inStream;
    inStream.vt.Read = MmapInStream_Read;
    inStream.data = (const Byte*)mapped;
    inStream.size = file_size;
    inStream.pos = 0;
    inStream.crc = &crc;
    inStream.fd = fd;
    
    /* Setup output stream */
    VolumeOutStream outStream;
    outStream.vt.Write = VolumeOutStream_Write;
    outStream.ctx = ctx;
    outStream.packed_size = &packed_size;
    outStream.buffer = out_buffer;
    outStream.buf_size = STREAM_BUFFER_SIZE;
    outStream.buf_pos = 0;
    
    /* Encode using streaming */
    res = Lzma2Enc_Encode2(enc,
        &outStream.vt, NULL, NULL,
        &inStream.vt, NULL, 0,
        NULL);
    
    if (res == SZ_OK) {
        VolumeOutStream_Flush(&outStream);
    }
    
    free(out_buffer);
    Lzma2Enc_Destroy(enc);
    munmap(mapped, file_size);
    close(fd);
    
    if (res != SZ_OK) return res;
    
    *out_crc = CRC_GET_DIGEST(crc);
    *out_packed_size = packed_size;
    return SZ_OK;

fallback_read:
    ; /* Empty statement after label */
#endif
    {
        /* Fallback: FILE* based reading */
        FILE* in_file = fopen(file_path, "rb");
        if (!in_file) return SZ_ERROR_READ;
        
        /* Use larger buffer for faster I/O */
        setvbuf(in_file, NULL, _IOFBF, 1024 * 1024);
        
        /* Create encoder */
        CLzma2EncHandle enc2 = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
        if (!enc2) {
            fclose(in_file);
            return SZ_ERROR_MEM;
        }
        
        /* Set expected data size - helps encoder optimize block threading */
        Lzma2Enc_SetDataSize(enc2, file_size);
        
        SRes res2 = Lzma2Enc_SetProps(enc2, props);
        if (res2 != SZ_OK) {
            Lzma2Enc_Destroy(enc2);
            fclose(in_file);
            return res2;
        }
        
        /* Get LZMA2 properties byte (for header, not for stream) */
        Byte prop2 = Lzma2Enc_WriteProperties(enc2);
        *out_prop = prop2;
        
        /* Initialize CRC */
        uint32_t crc2 = CRC_INIT_VAL;
        uint64_t packed_size2 = 0;
        
        /* Allocate I/O buffers for optimal performance */
        Byte* in_buffer = (Byte*)malloc(STREAM_BUFFER_SIZE);
        Byte* out_buffer2 = (Byte*)malloc(STREAM_BUFFER_SIZE);
        if (!in_buffer || !out_buffer2) {
            free(in_buffer);
            free(out_buffer2);
            Lzma2Enc_Destroy(enc2);
            fclose(in_file);
            return SZ_ERROR_MEM;
        }
        
        /* Setup input stream with buffering */
        FileInStream fileInStream;
        fileInStream.vt.Read = FileInStream_Read;
        fileInStream.file = in_file;
        fileInStream.remaining = file_size;
        fileInStream.crc = &crc2;
        fileInStream.buffer = in_buffer;
        fileInStream.buf_size = 0;
        fileInStream.buf_pos = 0;
        
        /* Setup output stream with buffering */
        VolumeOutStream outStream2;
        outStream2.vt.Write = VolumeOutStream_Write;
        outStream2.ctx = ctx;
        outStream2.packed_size = &packed_size2;
        outStream2.buffer = out_buffer2;
        outStream2.buf_size = STREAM_BUFFER_SIZE;
        outStream2.buf_pos = 0;
        
        /* Encode entire file using streaming interface */
        res2 = Lzma2Enc_Encode2(enc2,
            &outStream2.vt,    /* Output stream */
            NULL, NULL,       /* No output buffer */
            &fileInStream.vt, /* Input stream */
            NULL, 0,          /* No input buffer */
            NULL);            /* No progress */
        
        /* Flush any remaining data in output buffer */
        if (res2 == SZ_OK) {
            VolumeOutStream_Flush(&outStream2);
        }
        
        /* Cleanup */
        free(in_buffer);
        free(out_buffer2);
        Lzma2Enc_Destroy(enc2);
        fclose(in_file);
        
        if (res2 != SZ_OK) {
            return res2;
        }
        
        *out_crc = CRC_GET_DIGEST(crc2);
        *out_packed_size = packed_size2;
        
        return SZ_OK;
    }
}

/* Input stream that reads from multiple files sequentially (for solid compression) */
typedef struct {
    ISeqInStream vt;
    MV_FileEntry* files;
    size_t file_count;
    size_t current_file;
    FILE* current_fp;
    uint64_t current_file_remaining;
    uint32_t* file_crcs;  /* Array to store per-file CRCs */
    uint32_t current_crc;
    MultiVolumeContext* ctx;  /* For progress updates */
    SevenZipBytesProgressCallback progress_callback;
    void* user_data;
    uint64_t total_read;
    uint64_t total_size;
} SolidInStream;

static SRes SolidInStream_Read(ISeqInStreamPtr pp, void *buf, size_t *size) {
    SolidInStream *s = Z7_CONTAINER_FROM_VTBL(pp, SolidInStream, vt);
    size_t remaining = *size;
    Byte* out = (Byte*)buf;
    
    while (remaining > 0) {
        /* Open next file if needed */
        while (!s->current_fp || s->current_file_remaining == 0) {
            /* Finalize CRC of previous file */
            if (s->current_fp) {
                s->file_crcs[s->current_file] = CRC_GET_DIGEST(s->current_crc);
                fclose(s->current_fp);
                s->current_fp = NULL;
                s->current_file++;
            }
            
            /* Check if we've processed all files */
            if (s->current_file >= s->file_count) {
                *size -= remaining;
                return SZ_OK;
            }
            
            /* Open next file */
            MV_FileEntry* entry = &s->files[s->current_file];
            if (entry->is_dir || !entry->full_path) {
                s->file_crcs[s->current_file] = 0;
                s->current_file++;
                continue;
            }
            
            s->current_fp = fopen(entry->full_path, "rb");
            if (!s->current_fp) {
                return SZ_ERROR_READ;
            }
            setvbuf(s->current_fp, NULL, _IOFBF, 1024 * 1024);  /* 1MB buffer */
            s->current_file_remaining = entry->size;
            s->current_crc = CRC_INIT_VAL;
            
            /* Progress update - new file */
            if (s->progress_callback && s->ctx) {
                const char* name = entry->name ? entry->name : entry->full_path;
                s->progress_callback(s->total_read, s->total_size, 0, entry->size, name, s->user_data);
            }
        }
        
        /* Read from current file */
        size_t to_read = remaining;
        if (to_read > s->current_file_remaining) {
            to_read = (size_t)s->current_file_remaining;
        }
        
        size_t got = fread(out, 1, to_read, s->current_fp);
        if (got == 0) {
            /* Premature EOF - close file and move to next */
            s->file_crcs[s->current_file] = CRC_GET_DIGEST(s->current_crc);
            fclose(s->current_fp);
            s->current_fp = NULL;
            s->current_file++;
            continue;
        }
        
        /* Update CRC */
        s->current_crc = CrcUpdate(s->current_crc, out, got);
        
        /* Update progress */
        s->total_read += got;
        if (s->progress_callback && s->ctx) {
            MV_FileEntry* entry = &s->files[s->current_file];
            const char* name = entry->name ? entry->name : entry->full_path;
            uint64_t file_read = entry->size - s->current_file_remaining + got;
            s->progress_callback(s->total_read, s->total_size, file_read, entry->size, name, s->user_data);
        }
        
        s->current_file_remaining -= got;
        out += got;
        remaining -= got;
    }
    
    return SZ_OK;
}

/* Compress ALL files as a single solid stream - maximum parallelism */
static SRes compress_solid_streaming(
    MV_FileEntry* files,
    size_t file_count,
    uint64_t total_uncompressed_size,
    MultiVolumeContext* ctx,
    const CLzma2EncProps* props,
    uint64_t* out_packed_size,
    Byte* out_prop,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
) {
    /* Create encoder */
    CLzma2EncHandle enc = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
    if (!enc) return SZ_ERROR_MEM;
    
    /* Set expected data size for optimal block threading */
    Lzma2Enc_SetDataSize(enc, total_uncompressed_size);
    
    SRes res = Lzma2Enc_SetProps(enc, props);
    if (res != SZ_OK) {
        Lzma2Enc_Destroy(enc);
        return res;
    }
    
    *out_prop = Lzma2Enc_WriteProperties(enc);
    
    /* Allocate file CRC array */
    uint32_t* file_crcs = (uint32_t*)calloc(file_count, sizeof(uint32_t));
    if (!file_crcs) {
        Lzma2Enc_Destroy(enc);
        return SZ_ERROR_MEM;
    }
    
    /* Allocate output buffer */
    Byte* out_buffer = (Byte*)malloc(STREAM_BUFFER_SIZE);
    if (!out_buffer) {
        free(file_crcs);
        Lzma2Enc_Destroy(enc);
        return SZ_ERROR_MEM;
    }
    
    /* Setup solid input stream */
    SolidInStream inStream;
    inStream.vt.Read = SolidInStream_Read;
    inStream.files = files;
    inStream.file_count = file_count;
    inStream.current_file = 0;
    inStream.current_fp = NULL;
    inStream.current_file_remaining = 0;
    inStream.file_crcs = file_crcs;
    inStream.current_crc = CRC_INIT_VAL;
    inStream.ctx = ctx;
    inStream.progress_callback = progress_callback;
    inStream.user_data = user_data;
    inStream.total_read = 0;
    inStream.total_size = total_uncompressed_size;
    
    /* Setup output stream */
    uint64_t packed_size = 0;
    VolumeOutStream outStream;
    outStream.vt.Write = VolumeOutStream_Write;
    outStream.ctx = ctx;
    outStream.packed_size = &packed_size;
    outStream.buffer = out_buffer;
    outStream.buf_size = STREAM_BUFFER_SIZE;
    outStream.buf_pos = 0;
    
    /* Compress entire solid stream */
    res = Lzma2Enc_Encode2(enc,
        &outStream.vt, NULL, NULL,
        &inStream.vt, NULL, 0,
        NULL);
    
    if (res == SZ_OK) {
        VolumeOutStream_Flush(&outStream);
    }
    
    /* Close any remaining open file */
    if (inStream.current_fp) {
        inStream.file_crcs[inStream.current_file] = CRC_GET_DIGEST(inStream.current_crc);
        fclose(inStream.current_fp);
    }
    
    /* Copy CRCs back to file entries */
    for (size_t i = 0; i < file_count; i++) {
        files[i].crc = file_crcs[i];
        files[i].lzma2_prop = *out_prop;  /* All files share same LZMA2 prop in solid archive */
    }
    
    free(file_crcs);
    free(out_buffer);
    Lzma2Enc_Destroy(enc);
    
    *out_packed_size = packed_size;
    return res;
}

/* Build 7z header in memory */
static Byte* build_7z_header(
    MV_FileEntry* files,
    size_t file_count,
    uint64_t total_packed_size,
    size_t* header_size
) {
    size_t capacity = 256 * 1024;  /* 256KB buffer */
    Byte* header = (Byte*)malloc(capacity);
    if (!header) return NULL;
    
    Byte* p = header;
    
    *p++ = k7zIdHeader;
    
    /* MainStreamsInfo */
    *p++ = k7zIdMainStreamsInfo;
    
    /* PackInfo */
    *p++ = k7zIdPackInfo;
    WriteNumber(&p, 0);  /* Pack position */
    WriteNumber(&p, 1);  /* Number of pack streams */
    
    *p++ = k7zIdSize;
    fprintf(stderr, "[DEBUG build_7z_header] total_packed_size = %llu (0x%llX)\n", 
            (unsigned long long)total_packed_size, (unsigned long long)total_packed_size);
    WriteNumber(&p, total_packed_size);
    
    *p++ = k7zIdEnd;
    
    /* UnpackInfo */
    *p++ = k7zIdUnpackInfo;
    
    *p++ = k7zIdFolder;
    WriteNumber(&p, 1);  /* One folder */
    WriteNumber(&p, 0);  /* Not external */
    WriteNumber(&p, 1);  /* One coder */
    
    /* Check if using Copy/Store method (prop_byte == 0 means Copy) */
    Byte prop_byte = 0x01;  /* Default LZMA2 */
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].is_dir) {
            prop_byte = files[i].lzma2_prop;
            fprintf(stderr, "[DEBUG] File %s has lzma2_prop = 0x%02X\n", files[i].name, prop_byte);
            break;
        }
    }
    fprintf(stderr, "[DEBUG] Using prop_byte = 0x%02X, Copy mode = %s\n", prop_byte, prop_byte == 0 ? "YES" : "NO");
    
    if (prop_byte == 0) {
        /* Copy/Store method - no compression 
         * 7z Copy codec: ID = {0x00} (1 byte), no properties
         * Coder byte: 0x00 = simple coder, ID size = 0 (inline), no properties
         * But 7z expects at least 1 byte for codec ID
         */
        *p++ = 0x01;  /* Coder: ID size = 1, no NumInStreams/OutStreams, no properties */
        *p++ = 0x00;  /* Copy codec ID = 0x00 */
    } else {
        /* LZMA2 compression */
        *p++ = 0x21;  /* Coder flags (1 byte ID, has properties) */
        *p++ = 0x21;  /* LZMA2 codec ID */
        *p++ = 1;     /* Properties size = 1 byte */
        *p++ = prop_byte;
    }
    
    *p++ = k7zIdCodersUnpackSize;
    uint64_t total_unpack = 0;
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].is_dir) {
            total_unpack += files[i].size;
        }
    }
    WriteNumber(&p, total_unpack);
    
    *p++ = k7zIdEnd;
    
    /* SubStreamsInfo */
    *p++ = k7zIdSubStreamsInfo;
    
    size_t num_files = 0;
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].is_dir) num_files++;
    }
    
    /* Always emit NumUnpackStream (like working 7z_create.c) */
    *p++ = k7zIdNumUnpackStream;
    WriteNumber(&p, num_files);
    
    /* Individual file sizes (all but last - last is implied) */
    if (num_files > 1) {
        *p++ = k7zIdSize;
        size_t written = 0;
        for (size_t i = 0; i < file_count && written < num_files - 1; i++) {
            if (!files[i].is_dir) {
                WriteNumber(&p, files[i].size);
                written++;
            }
        }
    }
    
    *p++ = k7zIdCRC;
    *p++ = 1;  /* All defined */
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].is_dir) {
            memcpy(p, &files[i].crc, 4);
            p += 4;
        }
    }
    
    *p++ = k7zIdEnd;
    *p++ = k7zIdEnd;
    
    /* FilesInfo */
    *p++ = k7zIdFilesInfo;
    WriteNumber(&p, file_count);
    
    /* Names */
    *p++ = k7zIdName;
    size_t names_size = 0;
    for (size_t i = 0; i < file_count; i++) {
        names_size += (strlen(files[i].name) + 1) * 2;
    }
    WriteNumber(&p, names_size + 1);
    *p++ = 0;  /* Not external */
    
    for (size_t i = 0; i < file_count; i++) {
        const char* name = files[i].name;
        while (*name) {
            *p++ = (Byte)*name++;
            *p++ = 0;
        }
        *p++ = 0;
        *p++ = 0;
    }
    
    /* MTime (Modification Time) */
    *p++ = k7zIdMTime;
    WriteNumber(&p, file_count * 8 + 2);  /* Size: AllDefined(1) + External(1) + 8 bytes per file */
    *p++ = 1;  /* All defined */
    *p++ = 0;  /* External = 0 (inline data) */
    for (size_t i = 0; i < file_count; i++) {
        memcpy(p, &files[i].mtime, 8);
        p += 8;
    }
    
    /* WinAttrib (Windows Attributes) */
    *p++ = k7zIdWinAttrib;
    WriteNumber(&p, file_count * 4 + 2);  /* Size: AllDefined(1) + External(1) + 4 bytes per file */
    *p++ = 1;  /* All defined */
    *p++ = 0;  /* External = 0 (inline data) */
    for (size_t i = 0; i < file_count; i++) {
        memcpy(p, &files[i].attrib, 4);
        p += 4;
    }
    
    *p++ = k7zIdEnd;  /* End FilesInfo */
    *p++ = k7zIdEnd;  /* End Header */
    
    *header_size = p - header;
    return header;
}

/* Main multi-volume creation function */
SevenZipErrorCode sevenzip_create_multivolume_7z_complete(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const SevenZipStreamOptions* options,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !input_paths || !options || options->split_size == 0) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    CrcGenerateTable();
    
    /* Initialize context */
    MultiVolumeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.base_path, archive_path, sizeof(ctx.base_path) - 1);
    ctx.max_volume_size = options->split_size;
    ctx.progress_callback = progress_callback;
    ctx.user_data = user_data;
    ctx.volume_capacity = 8;
    ctx.volumes = (FILE**)malloc(ctx.volume_capacity * sizeof(FILE*));
    if (!ctx.volumes) {
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Gather file entries */
    size_t file_capacity = 256;
    size_t file_count = 0;
    MV_FileEntry* files = (MV_FileEntry*)malloc(file_capacity * sizeof(MV_FileEntry));
    if (!files) {
        free(ctx.volumes);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Helper to add a single file entry */
    #define ADD_FILE_ENTRY(full_path_str, relative_name_str) do { \
        if (file_count >= file_capacity) { \
            file_capacity *= 2; \
            MV_FileEntry* new_files = (MV_FileEntry*)realloc(files, file_capacity * sizeof(MV_FileEntry)); \
            if (!new_files) { \
                for (size_t _i = 0; _i < file_count; _i++) { free(files[_i].name); free(files[_i].full_path); } \
                free(files); \
                free(ctx.volumes); \
                return SEVENZIP_ERROR_MEMORY; \
            } \
            files = new_files; \
        } \
        MV_FileEntry* entry = &files[file_count++]; \
        memset(entry, 0, sizeof(MV_FileEntry)); \
        entry->name = strdup(relative_name_str); \
        entry->full_path = strdup(full_path_str); \
        struct STAT _st; \
        STAT(full_path_str, &_st); \
        entry->size = _st.st_size; \
        entry->mtime = ((uint64_t)_st.st_mtime * 10000000ULL) + 116444736000000000ULL; \
        entry->attrib = 0x20; \
        if (!(_st.st_mode & S_IWUSR)) entry->attrib |= 0x01; \
        entry->is_dir = 0; \
        ctx.total_size += _st.st_size; \
    } while(0)
    
    /* Process each input path - can be file or directory */
    for (int i = 0; input_paths[i] != NULL; i++) {
        struct STAT st;
        if (STAT(input_paths[i], &st) != 0) {
            continue;
        }
        
        if (S_ISREG(st.st_mode)) {
            /* Regular file - add directly */
            const char* basename = strrchr(input_paths[i], PATH_SEP);
            ADD_FILE_ENTRY(input_paths[i], basename ? basename + 1 : input_paths[i]);
        }
        else if (S_ISDIR(st.st_mode)) {
            /* Directory - recursively gather files */
            /* We'll use a simple recursive approach with a stack */
            char** dir_stack = (char**)malloc(256 * sizeof(char*));
            size_t stack_capacity = 256;
            size_t stack_size = 0;
            
            /* Get the base directory name for relative paths */
            const char* base_dir_name = strrchr(input_paths[i], PATH_SEP);
            base_dir_name = base_dir_name ? base_dir_name + 1 : input_paths[i];
            size_t base_path_len = strlen(input_paths[i]);
            
            /* Push initial directory */
            dir_stack[stack_size++] = strdup(input_paths[i]);
            
            while (stack_size > 0) {
                char* current_dir = dir_stack[--stack_size];
                
#ifdef _WIN32
                /* Windows directory iteration */
                char search_path[PATH_MAX];
                snprintf(search_path, PATH_MAX, "%s\\*", current_dir);
                WIN32_FIND_DATAA fd;
                HANDLE hFind = FindFirstFileA(search_path, &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
                        
                        char full_path[PATH_MAX];
                        snprintf(full_path, PATH_MAX, "%s\\%s", current_dir, fd.cFileName);
                        
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            if (stack_size >= stack_capacity) {
                                stack_capacity *= 2;
                                dir_stack = (char**)realloc(dir_stack, stack_capacity * sizeof(char*));
                            }
                            dir_stack[stack_size++] = strdup(full_path);
                        } else {
                            /* Build relative name: base_dir_name + path after base_path */
                            char relative_name[PATH_MAX];
                            snprintf(relative_name, PATH_MAX, "%s%s", base_dir_name, full_path + base_path_len);
                            ADD_FILE_ENTRY(full_path, relative_name);
                        }
                    } while (FindNextFileA(hFind, &fd));
                    FindClose(hFind);
                }
#else
                /* Unix directory iteration */
                DIR* dp = opendir(current_dir);
                if (dp) {
                    struct dirent* de;
                    while ((de = readdir(dp)) != NULL) {
                        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                        
                        char full_path[PATH_MAX];
                        snprintf(full_path, PATH_MAX, "%s/%s", current_dir, de->d_name);
                        
                        struct STAT child_st;
                        if (STAT(full_path, &child_st) != 0) continue;
                        
                        if (S_ISDIR(child_st.st_mode)) {
                            if (stack_size >= stack_capacity) {
                                stack_capacity *= 2;
                                dir_stack = (char**)realloc(dir_stack, stack_capacity * sizeof(char*));
                            }
                            dir_stack[stack_size++] = strdup(full_path);
                        } else if (S_ISREG(child_st.st_mode)) {
                            /* Build relative name: base_dir_name + path after base_path */
                            char relative_name[PATH_MAX];
                            snprintf(relative_name, PATH_MAX, "%s%s", base_dir_name, full_path + base_path_len);
                            ADD_FILE_ENTRY(full_path, relative_name);
                        }
                    }
                    closedir(dp);
                }
#endif
                free(current_dir);
            }
            free(dir_stack);
        }
    }
    
    #undef ADD_FILE_ENTRY
    
    if (file_count == 0) {
        free(files);
        free(ctx.volumes);
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Setup encoder properties - Let SDK choose optimal settings */
    CLzma2EncProps props;
    Lzma2EncProps_Init(&props);
    
    /* Map compression level - SDK will optimize based on level */
    int lzma_level;
    switch (level) {
        case SEVENZIP_LEVEL_STORE:
            lzma_level = 0;
            break;
        case SEVENZIP_LEVEL_FASTEST:
            lzma_level = 1;  /* Use level 1 for fastest with some compression */
            break;
        case SEVENZIP_LEVEL_FAST:
            lzma_level = 3;
            break;
        case SEVENZIP_LEVEL_NORMAL:
            lzma_level = 5;
            break;
        case SEVENZIP_LEVEL_MAXIMUM:
            lzma_level = 7;
            break;
        case SEVENZIP_LEVEL_ULTRA:
            lzma_level = 9;
            break;
        default:
            lzma_level = 5;
    }
    props.lzmaProps.level = lzma_level;
    
    /* Multi-threading - optimized for maximum CPU utilization */
    if (options->num_threads > 0) {
        props.numTotalThreads = options->num_threads;
        props.numBlockThreads_Max = options->num_threads;  /* Use all threads for block compression */
        props.lzmaProps.numThreads = 2;  /* 2 threads per LZMA stream (match finder + range coder) */
        props.blockSize = (1 << 26);  /* 64 MB blocks - better thread utilization */
    }
    
    /* Override dictionary if user specified one */
    if (options->dict_size > 0) {
        props.lzmaProps.dictSize = (UInt32)options->dict_size;
    }
    
    /* CRITICAL: Normalize will optimize all other parameters based on level */
    Lzma2EncProps_Normalize(&props);
    
    /* Reserve space for 7z signature and start header in first volume */
    FILE* first_vol_temp = open_new_volume(&ctx);
    if (!first_vol_temp) {
        for (size_t i = 0; i < file_count; i++) { free(files[i].name); free(files[i].full_path); }
        free(files);
        free(ctx.volumes);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Write signature */
    if (fwrite(k7zSignature, 1, k7zSignature_Size, first_vol_temp) != k7zSignature_Size) {
        goto error;
    }
    ctx.current_volume_size += k7zSignature_Size;
    
    /* Write version */
    Byte ver[2] = {0, 4};
    if (fwrite(ver, 1, 2, first_vol_temp) != 2) {
        goto error;
    }
    ctx.current_volume_size += 2;
    
    /* Placeholder for start header (32 bytes total after signature+version) */
    long start_header_pos = ftell(first_vol_temp);
    Byte start_header_data[24];
    memset(start_header_data, 0, 24);
    if (fwrite(start_header_data, 1, 24, first_vol_temp) != 24) {
        goto error;
    }
    ctx.current_volume_size += 24;
    
    /* Remember where packed data starts (after SignatureHeader) */
    long pack_start_pos = ftell(first_vol_temp);
    
    /* Reset byte tracking - only count packed data, not signature/start header */
    ctx.bytes_written = 0;  /* Reset for packed data tracking */
    ctx.total_packed_size = 0;
    
    /* Calculate total uncompressed size for solid stream */
    uint64_t total_uncompressed = 0;
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].is_dir) {
            total_uncompressed += files[i].size;
        }
    }
    
    /* Check if we're in Store (raw copy) mode */
    int use_store_mode = (level == SEVENZIP_LEVEL_STORE);
    
    /* Process each file */
    for (size_t i = 0; i < file_count; i++) {
        MV_FileEntry* file = &files[i];
        
        if (file->is_dir) {
            /* Directories: no data, just metadata */
            file->crc = 0;
            file->lzma2_prop = 0;
            continue;
        }
        
        uint32_t crc = 0;
        uint64_t packed_size = 0;
        SRes res;
        
        if (use_store_mode) {
            /* FAST PATH: Raw copy without compression (like 7z -mx=0) */
            file->lzma2_prop = 0;  /* 0 = Copy/Store method */
            res = store_file_uncompressed(
                file->full_path, NULL, file->size,
                &ctx, &crc, &packed_size);
        } else {
            /* Normal compression path */
            res = compress_file_streaming(
                file->full_path, &ctx, &props,
                &crc, &packed_size, &file->lzma2_prop);
        }
        
        if (res != SZ_OK) {
            fprintf(stderr, "Error compressing file: %s\n", file->name);
            goto error;
        }
        
        file->crc = crc;
        ctx.total_packed_size += packed_size;
    }
    
    /* Build header */
    size_t header_size = 0;
    Byte* header = build_7z_header(files, file_count, ctx.total_packed_size, &header_size);
    if (!header) {
        goto error;
    }
    
    /* Calculate header CRC before writing */
    uint32_t header_crc = CrcCalc(header, header_size);
    
    /* Write header to current position (end of packed data) */
    if (!write_across_volumes(&ctx, header, header_size)) {
        free(header);
        goto error;
    }
    free(header);
    
    /* Calculate NextHeader offset from end of SignatureHeader */
    /* SignatureHeader ends at pack_start_pos */
    /* NextHeader offset = size of all packed data */
    uint64_t next_header_offset = ctx.total_packed_size;
    uint64_t next_header_size = header_size;
    
    /* Build the StartHeader structure (NextHeaderOffset + NextHeaderSize + NextHeaderCRC) */
    Byte start_header_buf[20];
    memcpy(start_header_buf, &next_header_offset, 8);
    memcpy(start_header_buf + 8, &next_header_size, 8);
    memcpy(start_header_buf + 16, &header_crc, 4);
    
    /* Calculate StartHeader CRC */
    uint32_t start_header_crc = CrcCalc(start_header_buf, 20);
    
    /* Flush all volumes before seeking */
    for (size_t i = 0; i < ctx.volume_count; i++) {
        fflush(ctx.volumes[i]);
    }
    
    /* Go back and write start header in first volume (ALWAYS use ctx.volumes[0] after realloc!) */
    FILE* first_vol = ctx.volumes[0];  /* Get current pointer after any realloc */
    fseek(first_vol, start_header_pos, SEEK_SET);
    
    /* Write StartHeaderCRC */
    fwrite(&start_header_crc, 4, 1, first_vol);
    
    /* Write StartHeader data */
    fwrite(start_header_buf, 20, 1, first_vol);
    fflush(first_vol);
    
    /* Close all volumes */
    for (size_t i = 0; i < ctx.volume_count; i++) {
        fclose(ctx.volumes[i]);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < file_count; i++) {
        free(files[i].name);
        free(files[i].full_path);
    }
    free(files);
    free(ctx.volumes);
    
    return SEVENZIP_OK;
    
error:
    for (size_t i = 0; i < ctx.volume_count; i++) {
        fclose(ctx.volumes[i]);
    }
    for (size_t i = 0; i < file_count; i++) {
        free(files[i].name);
        free(files[i].full_path);
    }
    free(files);
    free(ctx.volumes);
    return SEVENZIP_ERROR_COMPRESS;
}
