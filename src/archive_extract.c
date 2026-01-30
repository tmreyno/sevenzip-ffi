#include "7z_ffi.h"
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zVersion.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
    #define PATH_SEPARATOR '\\'
#else
    #include <sys/types.h>
    #include <unistd.h>
    #define MKDIR(path) mkdir(path, 0755)
    #define PATH_SEPARATOR '/'
#endif

/* Create directory recursively */
static int create_directory_recursive(const char* path) {
    char* tmp = strdup(path);
    if (!tmp) return -1;
    
    size_t len = strlen(tmp);
    if (len == 0) {
        free(tmp);
        return -1;
    }
    
    if (tmp[len - 1] == PATH_SEPARATOR) {
        tmp[len - 1] = 0;
    }
    
    for (char* p = tmp + 1; *p; p++) {
        if (*p == PATH_SEPARATOR) {
            *p = 0;
            
            #ifdef _WIN32
            if (strlen(tmp) == 2 && tmp[1] == ':') {
                *p = PATH_SEPARATOR;
                continue;
            }
            #endif
            
            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (MKDIR(tmp) != 0 && errno != EEXIST) {
                    free(tmp);
                    return -1;
                }
            }
            
            *p = PATH_SEPARATOR;
        }
    }
    
    struct stat st;
    if (stat(tmp, &st) != 0) {
        if (MKDIR(tmp) != 0 && errno != EEXIST) {
            free(tmp);
            return -1;
        }
    }
    
    free(tmp);
    return 0;
}

/* Build output path */
static char* build_output_path(const char* output_dir, const char* filename) {
    size_t dir_len = strlen(output_dir);
    size_t file_len = strlen(filename);
    size_t total_len = dir_len + file_len + 2; /* +2 for separator and null terminator */
    
    char* path = (char*)malloc(total_len);
    if (!path) return NULL;
    
    snprintf(path, total_len, "%s%c%s", output_dir, PATH_SEPARATOR, filename);
    return path;
}

SevenZipErrorCode sevenzip_extract(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    if (!archive_path || !output_dir) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }
    
    /* Initialize CRC tables */
    CrcGenerateTable();
    
    /* Open archive file */
    CFileInStream archive_stream;
    CLookToRead2 look_stream;
    const size_t kInputBufSize = ((size_t)1 << 18);
    
    if (InFile_Open(&archive_stream.file, archive_path) != 0) {
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    FileInStream_CreateVTable(&archive_stream);
    
    /* Allocators */
    ISzAlloc alloc_imp = { SzAlloc, SzFree };
    ISzAlloc alloc_temp = { SzAllocTemp, SzFreeTemp };
    
    /* Initialize look stream */
    LookToRead2_CreateVTable(&look_stream, False);
    look_stream.buf = (Byte *)ISzAlloc_Alloc(&alloc_imp, kInputBufSize);
    if (!look_stream.buf) {
        File_Close(&archive_stream.file);
        return SEVENZIP_ERROR_MEMORY;
    }
    look_stream.bufSize = kInputBufSize;
    look_stream.realStream = &archive_stream.vt;
    LookToRead2_INIT(&look_stream);
    
    /* Initialize archive database */
    CSzArEx db;
    SzArEx_Init(&db);
    
    /* Open archive */
    SRes res = SzArEx_Open(&db, &look_stream.vt, &alloc_imp, &alloc_temp);
    if (res != SZ_OK) {
        File_Close(&archive_stream.file);
        SzArEx_Free(&db, &alloc_imp);
        return SEVENZIP_ERROR_INVALID_ARCHIVE;
    }
    
    /* Create output directory */
    if (create_directory_recursive(output_dir) != 0) {
        File_Close(&archive_stream.file);
        SzArEx_Free(&db, &alloc_imp);
        return SEVENZIP_ERROR_OPEN_FILE;
    }
    
    /* Extract all files */
    UInt32 block_index = 0xFFFFFFFF;
    Byte* out_buffer = NULL;
    size_t out_buffer_size = 0;
    
    SevenZipErrorCode error_code = SEVENZIP_OK;
    
    for (UInt32 i = 0; i < db.NumFiles; i++) {
        size_t offset = 0;
        size_t out_size_processed = 0;
        
        /* Get file info */
        size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
        if (len > 1) {
            UInt16* temp = (UInt16*)malloc(len * sizeof(UInt16));
            if (!temp) {
                error_code = SEVENZIP_ERROR_MEMORY;
                break;
            }
            
            SzArEx_GetFileNameUtf16(&db, i, temp);
            
            /* Convert UTF-16 to UTF-8 (simplified) */
            char* filename = (char*)malloc(len);
            if (!filename) {
                free(temp);
                error_code = SEVENZIP_ERROR_MEMORY;
                break;
            }
            
            for (size_t j = 0; j < len; j++) {
                filename[j] = (char)(temp[j] < 256 ? temp[j] : '?');
            }
            free(temp);
            
            /* Check if directory */
            BoolInt is_dir = SzArEx_IsDir(&db, i);
            
            /* Build output path */
            char* output_path = build_output_path(output_dir, filename);
            free(filename);
            
            if (!output_path) {
                error_code = SEVENZIP_ERROR_MEMORY;
                break;
            }
            
            if (is_dir) {
                /* Create directory */
                create_directory_recursive(output_path);
                free(output_path);
            } else {
                /* Extract file */
                res = SzArEx_Extract(&db, &look_stream.vt, i,
                                    &block_index, &out_buffer, &out_buffer_size,
                                    &offset, &out_size_processed,
                                    &alloc_imp, &alloc_temp);
                
                if (res != SZ_OK) {
                    free(output_path);
                    error_code = SEVENZIP_ERROR_EXTRACT;
                    break;
                }
                
                /* Create parent directories */
                char* last_sep = strrchr(output_path, PATH_SEPARATOR);
                if (last_sep) {
                    *last_sep = 0;
                    create_directory_recursive(output_path);
                    *last_sep = PATH_SEPARATOR;
                }
                
                /* Write file */
                FILE* output_file = fopen(output_path, "wb");
                if (!output_file) {
                    free(output_path);
                    error_code = SEVENZIP_ERROR_OPEN_FILE;
                    break;
                }
                
                size_t written = fwrite(out_buffer + offset, 1, out_size_processed, output_file);
                fclose(output_file);
                free(output_path);
                
                if (written != out_size_processed) {
                    error_code = SEVENZIP_ERROR_EXTRACT;
                    break;
                }
            }
            
            /* Progress callback */
            if (progress_callback) {
                progress_callback(i + 1, db.NumFiles, user_data);
            }
        }
    }
    
    /* Cleanup */
    if (out_buffer) {
        ISzAlloc_Free(&alloc_imp, out_buffer);
    }
    
    ISzAlloc_Free(&alloc_imp, look_stream.buf);
    SzArEx_Free(&db, &alloc_imp);
    File_Close(&archive_stream.file);
    
    return error_code;
}

SevenZipErrorCode sevenzip_extract_files(
    const char* archive_path,
    const char* output_dir,
    const char** files,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
) {
    /* TODO: Implement selective file extraction */
    return SEVENZIP_ERROR_NOT_IMPLEMENTED;
}
