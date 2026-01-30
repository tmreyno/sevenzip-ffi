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

SevenZipErrorCode sevenzip_list(
    const char* archive_path,
    const char* password,
    SevenZipList** list
) {
    if (!archive_path || !list) {
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
    
    /* Allocate result structure */
    SevenZipList* result = (SevenZipList*)malloc(sizeof(SevenZipList));
    if (!result) {
        File_Close(&archive_stream.file);
        SzArEx_Free(&db, &alloc_imp);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    result->count = db.NumFiles;
    result->entries = (SevenZipEntry*)calloc(db.NumFiles, sizeof(SevenZipEntry));
    
    if (!result->entries) {
        free(result);
        File_Close(&archive_stream.file);
        SzArEx_Free(&db, &alloc_imp);
        return SEVENZIP_ERROR_MEMORY;
    }
    
    /* Populate entry information */
    for (UInt32 i = 0; i < db.NumFiles; i++) {
        /* Get file name */
        size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
        if (len > 1) {
            UInt16* temp = (UInt16*)malloc(len * sizeof(UInt16));
            if (temp) {
                SzArEx_GetFileNameUtf16(&db, i, temp);
                
                /* Convert UTF-16 to UTF-8 (simplified) */
                result->entries[i].name = (char*)malloc(len);
                if (result->entries[i].name) {
                    for (size_t j = 0; j < len; j++) {
                        result->entries[i].name[j] = (char)(temp[j] < 256 ? temp[j] : '?');
                    }
                }
                free(temp);
            }
        }
        
        /* Get file size */
        result->entries[i].size = SzArEx_GetFileSize(&db, i);
        
        /* Get packed size (approximate) */
        result->entries[i].packed_size = 0; /* Would need to calculate from block info */
        
        /* Get modified time */
        if (SzBitWithVals_Check(&db.MTime, i)) {
            const CNtfsFileTime* ft = db.MTime.Vals + i;
            /* Convert Windows FILETIME to Unix timestamp (simplified) */
            result->entries[i].modified_time = (ft->Low | ((uint64_t)ft->High << 32)) / 10000000ULL - 11644473600ULL;
        } else {
            result->entries[i].modified_time = 0;
        }
        
        /* Get attributes */
        result->entries[i].attributes = 0;
        if (SzBitWithVals_Check(&db.Attribs, i)) {
            result->entries[i].attributes = db.Attribs.Vals[i];
        }
        
        /* Check if directory */
        result->entries[i].is_directory = SzArEx_IsDir(&db, i);
    }
    
    /* Cleanup */
    ISzAlloc_Free(&alloc_imp, look_stream.buf);
    SzArEx_Free(&db, &alloc_imp);
    File_Close(&archive_stream.file);
    
    *list = result;
    return SEVENZIP_OK;
}
