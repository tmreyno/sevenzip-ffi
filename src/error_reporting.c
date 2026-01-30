/**
 * Enhanced Error Reporting System for 7z FFI SDK
 * 
 * Provides detailed error context, file information, and actionable suggestions.
 * Thread-safe implementation using thread-local storage.
 */

#include "7z_ffi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Thread-local error context */
static pthread_key_t error_key;
static pthread_once_t error_key_once = PTHREAD_ONCE_INIT;

/* Initialize thread-local storage key */
static void make_error_key(void) {
    pthread_key_create(&error_key, free);
}

/* Get thread-local error info (creates if doesn't exist) */
static SevenZipErrorInfo* get_error_info(void) {
    pthread_once(&error_key_once, make_error_key);
    
    SevenZipErrorInfo* info = (SevenZipErrorInfo*)pthread_getspecific(error_key);
    if (!info) {
        info = (SevenZipErrorInfo*)calloc(1, sizeof(SevenZipErrorInfo));
        if (info) {
            info->code = SEVENZIP_OK;
            info->position = -1;
            pthread_setspecific(error_key, info);
        }
    }
    return info;
}

/**
 * Internal function to set detailed error information
 * Called by other modules when errors occur
 */
void sevenzip_set_error_internal(
    SevenZipErrorCode code,
    const char* message,
    const char* file_context,
    int64_t position,
    const char* suggestion
) {
    SevenZipErrorInfo* info = get_error_info();
    if (!info) return;
    
    info->code = code;
    info->position = position;
    
    /* Copy message */
    if (message) {
        strncpy(info->message, message, sizeof(info->message) - 1);
        info->message[sizeof(info->message) - 1] = '\0';
    } else {
        info->message[0] = '\0';
    }
    
    /* Copy file context */
    if (file_context) {
        strncpy(info->file_context, file_context, sizeof(info->file_context) - 1);
        info->file_context[sizeof(info->file_context) - 1] = '\0';
    } else {
        info->file_context[0] = '\0';
    }
    
    /* Copy suggestion */
    if (suggestion) {
        strncpy(info->suggestion, suggestion, sizeof(info->suggestion) - 1);
        info->suggestion[sizeof(info->suggestion) - 1] = '\0';
    } else {
        info->suggestion[0] = '\0';
    }
}

/* Public API implementation */

SevenZipErrorCode sevenzip_get_last_error(SevenZipErrorInfo* error_info) {
    if (!error_info) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    SevenZipErrorInfo* info = get_error_info();
    if (!info) {
        memset(error_info, 0, sizeof(SevenZipErrorInfo));
        error_info->code = SEVENZIP_ERROR_UNKNOWN;
        strncpy(error_info->message, "Failed to retrieve error information", sizeof(error_info->message) - 1);
        return SEVENZIP_ERROR_UNKNOWN;
    }
    
    memcpy(error_info, info, sizeof(SevenZipErrorInfo));
    return SEVENZIP_OK;
}

void sevenzip_clear_last_error(void) {
    SevenZipErrorInfo* info = get_error_info();
    if (info) {
        memset(info, 0, sizeof(SevenZipErrorInfo));
        info->code = SEVENZIP_OK;
        info->position = -1;
    }
}

const char* sevenzip_get_error_string(SevenZipErrorCode code) {
    switch (code) {
        case SEVENZIP_OK:
            return "Success";
        case SEVENZIP_ERROR_OPEN_FILE:
            return "Failed to open file - check file path and permissions";
        case SEVENZIP_ERROR_INVALID_ARCHIVE:
            return "Invalid or corrupted archive format";
        case SEVENZIP_ERROR_MEMORY:
            return "Memory allocation failed - insufficient RAM";
        case SEVENZIP_ERROR_EXTRACT:
            return "Extraction failed - possibly wrong password or corrupted data";
        case SEVENZIP_ERROR_COMPRESS:
            return "Compression failed - check disk space and file permissions";
        case SEVENZIP_ERROR_INVALID_PARAM:
            return "Invalid parameter provided to function";
        case SEVENZIP_ERROR_NOT_IMPLEMENTED:
            return "Feature not implemented";
        case SEVENZIP_ERROR_UNKNOWN:
        default:
            return "Unknown error occurred";
    }
}

/* Helper macros for setting errors in other modules */
#define SET_ERROR_SIMPLE(code, msg) \
    sevenzip_set_error_internal(code, msg, NULL, -1, NULL)

#define SET_ERROR_WITH_FILE(code, msg, file) \
    sevenzip_set_error_internal(code, msg, file, -1, NULL)

#define SET_ERROR_FULL(code, msg, file, pos, suggestion) \
    sevenzip_set_error_internal(code, msg, file, pos, suggestion)

/* Example usage in extraction code:
 *
 * if (fopen(path, "rb") == NULL) {
 *     SET_ERROR_FULL(
 *         SEVENZIP_ERROR_OPEN_FILE,
 *         "Failed to open archive file",
 *         path,
 *         0,
 *         "Check if file exists and you have read permission: ls -la"
 *     );
 *     return SEVENZIP_ERROR_OPEN_FILE;
 * }
 */

/* Example suggestions for common errors */

static void set_file_open_error(const char* path, int for_reading) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Failed to open file: %s", path);
    
    const char* suggestion = for_reading 
        ? "Verify file exists: ls -la \"filename\"\nCheck read permissions: chmod +r \"filename\""
        : "Check write permissions and disk space: df -h\nVerify parent directory exists";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_OPEN_FILE, msg, path, 0, suggestion);
}

static void set_invalid_archive_error(const char* path, int64_t position) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Invalid archive format at byte %lld", (long long)position);
    
    const char* suggestion = 
        "Verify archive integrity: 7z t archive.7z\n"
        "Check if file is really a 7z archive: file archive.7z\n"
        "Try extracting with official 7-Zip to compare";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_INVALID_ARCHIVE, msg, path, position, suggestion);
}

static void set_extraction_error(const char* path, const char* entry_name) {
    char msg[256];
    char context[256];
    snprintf(msg, sizeof(msg), "Failed to extract: %s", entry_name);
    snprintf(context, sizeof(context), "%s:%s", path, entry_name);
    
    const char* suggestion = 
        "If encrypted, verify password is correct\n"
        "Check disk space: df -h\n"
        "Verify output directory is writable";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_EXTRACT, msg, context, -1, suggestion);
}

static void set_compression_error(const char* input_path, const char* archive_path) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Failed to compress: %s", input_path);
    
    const char* suggestion = 
        "Check input file is readable: ls -la \"file\"\n"
        "Verify sufficient disk space: df -h\n"
        "Ensure output directory is writable";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_COMPRESS, msg, input_path, -1, suggestion);
}

static void set_wrong_password_error(const char* archive_path) {
    const char* msg = "Wrong password or corrupted encrypted data";
    
    const char* suggestion = 
        "Verify password is correct (case-sensitive)\n"
        "If archive is not encrypted, don't provide password\n"
        "Check if archive is corrupted: 7z t archive.7z";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_EXTRACT, msg, archive_path, -1, suggestion);
}

static void set_memory_error(size_t requested_bytes) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Failed to allocate %zu bytes", requested_bytes);
    
    const char* suggestion = 
        "Check available memory: free -h (Linux) or vm_stat (macOS)\n"
        "Close other applications to free RAM\n"
        "Consider using streaming mode for large files";
    
    SET_ERROR_FULL(SEVENZIP_ERROR_MEMORY, msg, NULL, -1, suggestion);
}

/* Export helper functions for use by other modules */
void sevenzip_error_file_open(const char* path, int for_reading) {
    set_file_open_error(path, for_reading);
}

void sevenzip_error_invalid_archive(const char* path, int64_t position) {
    set_invalid_archive_error(path, position);
}

void sevenzip_error_extraction(const char* path, const char* entry) {
    set_extraction_error(path, entry);
}

void sevenzip_error_compression(const char* input, const char* archive) {
    set_compression_error(input, archive);
}

void sevenzip_error_wrong_password(const char* archive) {
    set_wrong_password_error(archive);
}

void sevenzip_error_memory(size_t bytes) {
    set_memory_error(bytes);
}
