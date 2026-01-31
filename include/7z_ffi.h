#ifndef SEVENZIP_FFI_H
#define SEVENZIP_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Export macros for shared library */
#if defined(_WIN32) || defined(_WIN64)
    #ifdef DLL_EXPORT
        #define SEVENZIP_API __declspec(dllexport)
    #else
        #define SEVENZIP_API __declspec(dllimport)
    #endif
#else
    #define SEVENZIP_API __attribute__((visibility("default")))
#endif

/* Error codes */
typedef enum {
    SEVENZIP_OK = 0,
    SEVENZIP_ERROR_OPEN_FILE = 1,
    SEVENZIP_ERROR_INVALID_ARCHIVE = 2,
    SEVENZIP_ERROR_MEMORY = 3,
    SEVENZIP_ERROR_EXTRACT = 4,
    SEVENZIP_ERROR_COMPRESS = 5,
    SEVENZIP_ERROR_INVALID_PARAM = 6,
    SEVENZIP_ERROR_NOT_IMPLEMENTED = 7,
    SEVENZIP_ERROR_UNKNOWN = 99
} SevenZipErrorCode;

/* Archive entry information */
typedef struct {
    char* name;              /* File name (UTF-8) */
    uint64_t size;           /* Uncompressed size */
    uint64_t packed_size;    /* Compressed size */
    uint64_t modified_time;  /* Unix timestamp */
    uint32_t attributes;     /* File attributes */
    int is_directory;        /* 1 if directory, 0 if file */
} SevenZipEntry;

/* Archive list result */
typedef struct {
    SevenZipEntry* entries;  /* Array of entries */
    size_t count;            /* Number of entries */
} SevenZipList;

/* Progress callback function type */
typedef void (*SevenZipProgressCallback)(uint64_t completed, uint64_t total, void* user_data);

/* Byte-level progress callback for streaming operations */
typedef void (*SevenZipBytesProgressCallback)(
    uint64_t bytes_processed,      /* Total bytes processed so far */
    uint64_t bytes_total,           /* Total bytes to process (0 if unknown) */
    uint64_t current_file_bytes,    /* Bytes processed in current file */
    uint64_t current_file_total,    /* Total bytes in current file */
    const char* current_file_name,  /* Name of file being processed */
    void* user_data
);

/* Compression level */
typedef enum {
    SEVENZIP_LEVEL_STORE = 0,      /* No compression */
    SEVENZIP_LEVEL_FASTEST = 1,    /* Fastest compression */
    SEVENZIP_LEVEL_FAST = 3,       /* Fast compression */
    SEVENZIP_LEVEL_NORMAL = 5,     /* Normal compression */
    SEVENZIP_LEVEL_MAXIMUM = 7,    /* Maximum compression */
    SEVENZIP_LEVEL_ULTRA = 9       /* Ultra compression */
} SevenZipCompressionLevel;

/* Advanced compression options */
typedef struct {
    int num_threads;           /* Number of threads (0 = auto, default: 2) */
    uint64_t dict_size;        /* Dictionary size in bytes (0 = auto) */
    int solid;                 /* Solid archive (1 = yes, 0 = no, default: 1) */
    const char* password;      /* Password for encryption (NULL = no encryption) */
} SevenZipCompressOptions;

/* Streaming compression options for large files and split archives */
typedef struct {
    int num_threads;           /* Number of threads (0 = auto, default: 2) */
    uint64_t dict_size;        /* Dictionary size in bytes (0 = auto, default: 32MB) */
    int solid;                 /* Solid archive (1 = yes, 0 = no, default: 1) */
    const char* password;      /* Password for encryption (NULL = no encryption) */
    uint64_t split_size;       /* Split archive size in bytes (0 = no split, e.g., 4GB = 4294967296) */
    uint64_t chunk_size;       /* Chunk size for streaming (0 = auto, default: 64MB) */
    const char* temp_dir;      /* Temporary directory (NULL = system default) */
    int delete_temp_on_error;  /* Delete temp files on error (1 = yes, 0 = no, default: 1) */
} SevenZipStreamOptions;

/**
 * Initialize the 7z library
 * Call this before any other functions
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_init(void);

/**
 * Cleanup the 7z library
 * Call this when done using the library
 */
SEVENZIP_API void sevenzip_cleanup(void);

/**
 * Extract a 7z archive
 * @param archive_path Path to the archive file
 * @param output_dir Directory to extract to
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_extract(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Extract specific files from a 7z archive
 * @param archive_path Path to the archive file
 * @param output_dir Directory to extract to
 * @param files Array of file names to extract (NULL-terminated)
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_extract_files(
    const char* archive_path,
    const char* output_dir,
    const char** files,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Create a 7z archive
 * @param archive_path Path for the new archive file
 * @param input_paths Array of file/directory paths to compress (NULL-terminated)
 * @param level Compression level
 * @param password Optional password (NULL for no encryption)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_compress(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Create a multi-file archive with LZMA2 compression
 * @param archive_path Path for the new archive file
 * @param input_paths Array of file paths to compress (NULL-terminated)
 * @param level Compression level
 * @param password Optional password (NULL for no encryption - not yet implemented)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_create_archive(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Create a standard .7z archive (compatible with 7-Zip)
 * Uses LZMA2 compression and creates archives readable by official 7-Zip
 * Supports: directories, large files (>4GB), multi-threading, solid compression
 * @param archive_path Path for the output .7z file
 * @param input_paths Array of file/directory paths to compress (NULL-terminated)
 * @param level Compression level
 * @param options Advanced options (NULL for defaults: 2 threads, solid, no password)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_create_7z(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const SevenZipCompressOptions* options,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Extract a multi-file archive created with sevenzip_create_archive()
 * @param archive_path Path to the archive file
 * @param output_dir Directory to extract to
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_extract_archive(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * List contents of a 7z archive
 * @param archive_path Path to the archive file
 * @param password Optional password (NULL if not encrypted)
 * @param list Pointer to receive the list result (must be freed with sevenzip_free_list)
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_list(
    const char* archive_path,
    const char* password,
    SevenZipList** list
);

/**
 * Free memory allocated by sevenzip_list
 * @param list List to free
 */
SEVENZIP_API void sevenzip_free_list(SevenZipList* list);

/**
 * Test archive integrity without extracting
 * Validates CRCs, decompression, and structure without writing files to disk
 * @param archive_path Path to the archive file (supports split volumes)
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK if archive is valid, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_test_archive(
    const char* archive_path,
    const char* password,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
);

/**
 * Decompress a standalone LZMA file (.lzma)
 * @param lzma_path Path to the .lzma file
 * @param output_path Path for the decompressed output file
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_decompress_lzma(
    const char* lzma_path,
    const char* output_path,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Decompress a standalone LZMA2 file (.xz or custom format)
 * @param lzma2_path Path to the LZMA2 file
 * @param output_path Path for the decompressed output file
 * @param progress_callback Optional progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_decompress_lzma2(
    const char* lzma2_path,
    const char* output_path,
    SevenZipProgressCallback progress_callback,
    void* user_data
);

/**
 * Get error message for error code
 * @param error_code Error code
 * @return Human-readable error message
 */
SEVENZIP_API const char* sevenzip_get_error_message(SevenZipErrorCode error_code);

/**
 * Get library version
 * @return Version string (e.g., "1.0.0")
 */
SEVENZIP_API const char* sevenzip_get_version(void);

/**
 * Initialize streaming options with defaults
 * @param options Pointer to options structure to initialize
 */
SEVENZIP_API void sevenzip_stream_options_init(SevenZipStreamOptions* options);

/**
 * Create a 7z archive with streaming compression (handles large files and splits)
 * This function processes files in chunks to avoid loading entire files into RAM.
 * Supports split/multi-volume archives for easier transfer and storage.
 * 
 * @param archive_path Base path for the archive (e.g., "archive.7z")
 *                     For split archives, creates archive.7z.001, archive.7z.002, etc.
 * @param input_paths Array of file/directory paths to compress (NULL-terminated)
 * @param level Compression level (0-9)
 * @param options Streaming options (NULL for defaults)
 * @param progress_callback Optional byte-level progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 * 
 * Example:
 *   SevenZipStreamOptions opts;
 *   sevenzip_stream_options_init(&opts);
 *   opts.split_size = 4294967296;  // 4GB segments
 *   opts.chunk_size = 67108864;     // 64MB chunks
 *   opts.num_threads = 8;
 *   
 *   const char* files[] = {"/path/to/large/file.img", NULL};
 *   sevenzip_create_7z_streaming("archive.7z", files, 
 *                                  SEVENZIP_LEVEL_NORMAL, &opts, 
 *                                  my_progress_callback, NULL);
 */
SEVENZIP_API SevenZipErrorCode sevenzip_create_7z_streaming(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const SevenZipStreamOptions* options,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
);

/**
 * Create a 7z archive with TRUE streaming compression
 * 
 * ⚠️ IMPORTANT: This function processes files in 64MB chunks WITHOUT loading
 * all data into RAM first. This is essential for large archives (10GB+) that
 * would otherwise cause out-of-memory crashes.
 * 
 * Memory usage: ~250MB peak regardless of archive size
 * 
 * @param archive_path Output archive path
 * @param input_paths Array of file/directory paths to compress (NULL-terminated)
 * @param level Compression level
 * @param options Streaming options (NULL for defaults)
 * @param progress_callback Byte-level progress callback (NULL to disable)
 * @param user_data User data for callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_create_7z_true_streaming(
    const char* archive_path,
    const char** input_paths,
    SevenZipCompressionLevel level,
    const SevenZipStreamOptions* options,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
);

/**
 * Extract a 7z archive with streaming decompression and byte-level progress
 * Handles split/multi-volume archives automatically.
 * 
 * @param archive_path Path to archive (for splits, use base name like "archive.7z.001")
 * @param output_dir Directory to extract to
 * @param password Optional password (NULL if not encrypted)
 * @param progress_callback Optional byte-level progress callback (NULL to disable)
 * @param user_data User data passed to progress callback
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_extract_streaming(
    const char* archive_path,
    const char* output_dir,
    const char* password,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
);

/**
 * ============================================================================
 * AES-256 Encryption Functions
 * ============================================================================
 */

/* AES constants */
#define AES_KEY_SIZE 32          /* 256 bits */
#define AES_BLOCK_SIZE 16        /* 128 bits */
#define AES_NUM_IVMRK_WORDS ((1 + 1 + 15) * 4)  /* IV + keyMode + roundKeys */

/**
 * Initialize encryption context with password
 * 
 * Derives a 256-bit AES key from the password using PBKDF2-SHA256
 * with 262,144 iterations (7-Zip standard).
 * 
 * @param password Password string (UTF-8)
 * @param key Output buffer for derived key (32 bytes)
 * @param iv Output buffer for initialization vector (16 bytes, randomly generated)
 * @param aes_context Output buffer for AES context (must be 16-byte aligned)
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_init_encryption(
    const char* password,
    uint8_t* key,
    uint8_t* iv,
    uint32_t* aes_context
);

/**
 * Initialize decryption context with password and salt
 * 
 * @param password Password string (UTF-8)
 * @param salt Salt used for key derivation (from archive header)
 * @param salt_len Length of salt in bytes (typically 16)
 * @param key Output buffer for derived key (32 bytes)
 * @param aes_context Output buffer for AES context (must be 16-byte aligned)
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_init_decryption(
    const char* password,
    const uint8_t* salt,
    size_t salt_len,
    uint8_t* key,
    uint32_t* aes_context
);

/**
 * Encrypt data using AES-256-CBC with PKCS#7 padding
 * 
 * @param aes_context AES context from sevenzip_init_encryption()
 * @param iv Initialization vector (16 bytes)
 * @param plaintext Data to encrypt
 * @param plaintext_len Length of plaintext in bytes
 * @param ciphertext Output buffer for encrypted data (must be 16-byte aligned)
 * @param ciphertext_len In: buffer size, Out: actual encrypted length
 * @return SEVENZIP_OK on success, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_encrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
);

/**
 * Decrypt data using AES-256-CBC and verify PKCS#7 padding
 * 
 * @param aes_context AES context from sevenzip_init_decryption()
 * @param iv Initialization vector (16 bytes, from archive header)
 * @param ciphertext Encrypted data
 * @param ciphertext_len Length of ciphertext in bytes (must be multiple of 16)
 * @param plaintext Output buffer for decrypted data (must be 16-byte aligned)
 * @param plaintext_len In: buffer size, Out: actual decrypted length
 * @return SEVENZIP_OK on success, SEVENZIP_ERROR_EXTRACT if wrong password
 */
SEVENZIP_API SevenZipErrorCode sevenzip_decrypt_data(
    uint32_t* aes_context,
    const uint8_t* iv,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* plaintext,
    size_t* plaintext_len
);

/**
 * Verify password correctness by decrypting test block
 * 
 * @param password Password to verify
 * @param encrypted_test_block Encrypted test data
 * @param test_block_len Length of test block
 * @param salt Salt used for key derivation
 * @param salt_len Length of salt
 * @param iv Initialization vector
 * @return SEVENZIP_OK if password correct, error code otherwise
 */
SEVENZIP_API SevenZipErrorCode sevenzip_verify_password(
    const char* password,
    const uint8_t* encrypted_test_block,
    size_t test_block_len,
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* iv
);

/* ============================================================================
 * Enhanced Error Reporting
 * ============================================================================ */

/**
 * Detailed error information structure
 */
typedef struct {
    SevenZipErrorCode code;           /* Error code */
    char message[512];                 /* Error message */
    char file_context[256];            /* File being processed when error occurred */
    int64_t position;                  /* Position in file/archive (-1 if N/A) */
    char suggestion[256];              /* Actionable suggestion to fix the error */
} SevenZipErrorInfo;

/**
 * Get detailed information about the last error
 * Thread-safe: each thread has its own error context
 * 
 * @param error_info Output structure to fill with error details
 * @return SEVENZIP_OK if error info retrieved, SEVENZIP_ERROR_INVALID_PARAM if NULL
 */
SEVENZIP_API SevenZipErrorCode sevenzip_get_last_error(SevenZipErrorInfo* error_info);

/**
 * Clear the last error information
 * Useful for debugging to ensure you're seeing fresh errors
 */
SEVENZIP_API void sevenzip_clear_last_error(void);

/**
 * Get a human-readable error message for an error code
 * 
 * @param code Error code
 * @return Static string describing the error (never NULL)
 */
SEVENZIP_API const char* sevenzip_get_error_string(SevenZipErrorCode code);

#ifdef __cplusplus
}
#endif

#endif /* SEVENZIP_FFI_H */
