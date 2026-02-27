/**
 * Forensic Manifest Generator for 7z Archives
 *
 * Generates a `.forensic-manifest.json` file containing per-file metadata:
 *   - SHA-256 hash
 *   - Original absolute path
 *   - All timestamps (mtime, ctime/birth, atime) in ISO 8601
 *   - Unix permissions (uid, gid, mode)
 *   - Extended attributes (macOS/Linux)
 *   - POSIX ACLs (macOS/Linux)
 *   - Source label and creation metadata
 *
 * Uses LZMA SDK's SHA-256 implementation (hardware-accelerated on ARM64).
 */

#include "../include/7z_ffi.h"
#include "../lzma/C/Sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #define STAT _stat
    #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#else
    #include <unistd.h>
    #include <pwd.h>
    #include <grp.h>
    #include <dirent.h>
    #define STAT stat
    #if defined(__APPLE__)
        #include <sys/xattr.h>
        #include <sys/acl.h>
    #elif defined(__linux__)
        #include <sys/xattr.h>
        #include <sys/acl.h>
    #endif
#endif

/* SHA-256 hash buffer size */
#define SHA256_HEX_SIZE 65  /* 64 hex chars + null */

/* Read buffer for hashing */
#define HASH_READ_BUF_SIZE (1024 * 1024)  /* 1MB */

/* Maximum manifest entries */
#define MAX_MANIFEST_ENTRIES 100000

/* JSON string escaping buffer multiplier */
#define JSON_ESCAPE_MULTIPLIER 6  /* worst case: each char becomes \uXXXX */

/* Forensic manifest entry */
typedef struct {
    char* archive_name;      /* Name as stored in archive */
    char* original_path;     /* Original absolute path on source system */
    char sha256[SHA256_HEX_SIZE];  /* SHA-256 hash hex string */
    uint64_t size;           /* File size in bytes */
    char mtime_iso[32];      /* Modification time ISO 8601 */
    char ctime_iso[32];      /* Creation time ISO 8601 */
    char atime_iso[32];      /* Access time ISO 8601 */
    uint32_t uid;
    uint32_t gid;
    uint16_t mode;           /* Unix permission bits */
    char* acl_text;          /* POSIX ACL text representation (or NULL) */
    char** xattr_names;      /* Extended attribute names */
    char** xattr_values;     /* Extended attribute values (base64 or hex) */
    size_t xattr_count;
    int is_dir;
} ForensicManifestEntry;

/* Forensic manifest */
typedef struct {
    ForensicManifestEntry* entries;
    size_t count;
    size_t capacity;
    char* source_label;      /* e.g., "MacBook Air - Case 2024-001" */
    char* tool_name;         /* "CORE-FFX" */
    char* tool_version;      /* e.g., "1.0.0" */
    char created_iso[32];    /* Manifest creation time */
} ForensicManifest;

/* ---- Utility functions ---- */

/* Convert Unix timestamp to ISO 8601 string */
static void unix_to_iso8601(time_t t, char* buf, size_t buf_size) {
    if (t <= 0) {
        snprintf(buf, buf_size, "");
        return;
    }
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
}

/* Compute SHA-256 hash of a file, returns 0 on success */
static int compute_sha256(const char* path, char* hex_out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(hex_out, SHA256_HEX_SIZE, "");
        return -1;
    }

    /* Initialize LZMA SDK SHA-256 */
    Sha256Prepare();
    CSha256 sha;
    Sha256_Init(&sha);

    unsigned char* buf = (unsigned char*)malloc(HASH_READ_BUF_SIZE);
    if (!buf) {
        fclose(f);
        return -1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, HASH_READ_BUF_SIZE, f)) > 0) {
        Sha256_Update(&sha, buf, bytes_read);
    }

    fclose(f);
    free(buf);

    unsigned char digest[SHA256_DIGEST_SIZE];
    Sha256_Final(&sha, digest);

    /* Convert to hex string */
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        snprintf(hex_out + i * 2, 3, "%02x", digest[i]);
    }
    hex_out[SHA256_DIGEST_SIZE * 2] = '\0';

    return 0;
}

/* Escape a string for JSON output */
static char* json_escape(const char* str) {
    if (!str) return strdup("");

    size_t len = strlen(str);
    size_t max_len = len * JSON_ESCAPE_MULTIPLIER + 1;
    char* out = (char*)malloc(max_len);
    if (!out) return strdup("");

    char* p = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        switch (c) {
            case '"':  *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b'; break;
            case '\f': *p++ = '\\'; *p++ = 'f'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default:
                if (c < 0x20) {
                    p += snprintf(p, 7, "\\u%04x", c);
                } else {
                    *p++ = (char)c;
                }
                break;
        }
    }
    *p = '\0';
    return out;
}

/* Convert binary data to hex string */
static char* to_hex_string(const unsigned char* data, size_t len) {
    char* hex = (char*)malloc(len * 2 + 1);
    if (!hex) return strdup("");
    for (size_t i = 0; i < len; i++) {
        snprintf(hex + i * 2, 3, "%02x", data[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

#if !defined(_WIN32)
/* Get POSIX ACL as text string */
static char* get_acl_text(const char* path) {
#if defined(__APPLE__) || defined(__linux__)
    acl_t acl = acl_get_file(path, ACL_TYPE_ACCESS);
    if (!acl) return NULL;

    char* text = acl_to_text(acl, NULL);
    char* result = NULL;
    if (text) {
        result = strdup(text);
        acl_free(text);
    }
    acl_free(acl);
    return result;
#else
    (void)path;
    return NULL;
#endif
}

/* Get extended attributes */
static void get_xattrs(const char* path, char*** names_out, char*** values_out, size_t* count_out) {
    *names_out = NULL;
    *values_out = NULL;
    *count_out = 0;

#if defined(__APPLE__)
    /* macOS: listxattr/getxattr without options */
    ssize_t list_len = listxattr(path, NULL, 0, XATTR_NOFOLLOW);
    if (list_len <= 0) return;

    char* list_buf = (char*)malloc(list_len);
    if (!list_buf) return;

    ssize_t actual = listxattr(path, list_buf, list_len, XATTR_NOFOLLOW);
    if (actual <= 0) { free(list_buf); return; }

    /* Count names (null-separated) */
    size_t count = 0;
    for (ssize_t i = 0; i < actual; i++) {
        if (list_buf[i] == '\0') count++;
    }
    if (count == 0) { free(list_buf); return; }

    *names_out = (char**)calloc(count, sizeof(char*));
    *values_out = (char**)calloc(count, sizeof(char*));
    if (!*names_out || !*values_out) {
        free(list_buf);
        free(*names_out); *names_out = NULL;
        free(*values_out); *values_out = NULL;
        return;
    }

    size_t idx = 0;
    const char* name = list_buf;
    for (ssize_t i = 0; i < actual && idx < count; i++) {
        if (list_buf[i] == '\0') {
            (*names_out)[idx] = strdup(name);

            /* Get value */
            ssize_t val_len = getxattr(path, name, NULL, 0, 0, XATTR_NOFOLLOW);
            if (val_len > 0 && val_len <= 65536) {
                unsigned char* val_buf = (unsigned char*)malloc(val_len);
                if (val_buf) {
                    ssize_t got = getxattr(path, name, val_buf, val_len, 0, XATTR_NOFOLLOW);
                    if (got > 0) {
                        (*values_out)[idx] = to_hex_string(val_buf, got);
                    } else {
                        (*values_out)[idx] = strdup("");
                    }
                    free(val_buf);
                } else {
                    (*values_out)[idx] = strdup("");
                }
            } else {
                (*values_out)[idx] = strdup("");
            }

            idx++;
            name = list_buf + i + 1;
        }
    }
    *count_out = idx;
    free(list_buf);

#elif defined(__linux__)
    /* Linux: listxattr/getxattr (no options param) */
    ssize_t list_len = listxattr(path, NULL, 0);
    if (list_len <= 0) return;

    char* list_buf = (char*)malloc(list_len);
    if (!list_buf) return;

    ssize_t actual = listxattr(path, list_buf, list_len);
    if (actual <= 0) { free(list_buf); return; }

    size_t count = 0;
    for (ssize_t i = 0; i < actual; i++) {
        if (list_buf[i] == '\0') count++;
    }
    if (count == 0) { free(list_buf); return; }

    *names_out = (char**)calloc(count, sizeof(char*));
    *values_out = (char**)calloc(count, sizeof(char*));
    if (!*names_out || !*values_out) {
        free(list_buf);
        free(*names_out); *names_out = NULL;
        free(*values_out); *values_out = NULL;
        return;
    }

    size_t idx = 0;
    const char* name = list_buf;
    for (ssize_t i = 0; i < actual && idx < count; i++) {
        if (list_buf[i] == '\0') {
            (*names_out)[idx] = strdup(name);

            ssize_t val_len = getxattr(path, name, NULL, 0);
            if (val_len > 0 && val_len <= 65536) {
                unsigned char* val_buf = (unsigned char*)malloc(val_len);
                if (val_buf) {
                    ssize_t got = getxattr(path, name, val_buf, val_len);
                    if (got > 0) {
                        (*values_out)[idx] = to_hex_string(val_buf, got);
                    } else {
                        (*values_out)[idx] = strdup("");
                    }
                    free(val_buf);
                } else {
                    (*values_out)[idx] = strdup("");
                }
            } else {
                (*values_out)[idx] = strdup("");
            }

            idx++;
            name = list_buf + i + 1;
        }
    }
    *count_out = idx;
    free(list_buf);
#endif
}
#endif /* !_WIN32 */

/* ---- Manifest building ---- */

static ForensicManifest* manifest_create(const char* source_label) {
    ForensicManifest* m = (ForensicManifest*)calloc(1, sizeof(ForensicManifest));
    if (!m) return NULL;

    m->capacity = 256;
    m->entries = (ForensicManifestEntry*)calloc(m->capacity, sizeof(ForensicManifestEntry));
    if (!m->entries) { free(m); return NULL; }

    m->source_label = source_label ? strdup(source_label) : strdup("unknown");
    m->tool_name = strdup("CORE-FFX");
    m->tool_version = strdup("1.0.0");

    /* Record manifest creation time */
    time_t now = time(NULL);
    unix_to_iso8601(now, m->created_iso, sizeof(m->created_iso));

    return m;
}

static void manifest_free(ForensicManifest* m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        ForensicManifestEntry* e = &m->entries[i];
        free(e->archive_name);
        free(e->original_path);
        free(e->acl_text);
        for (size_t j = 0; j < e->xattr_count; j++) {
            free(e->xattr_names[j]);
            free(e->xattr_values[j]);
        }
        free(e->xattr_names);
        free(e->xattr_values);
    }
    free(m->entries);
    free(m->source_label);
    free(m->tool_name);
    free(m->tool_version);
    free(m);
}

static int manifest_add_file(ForensicManifest* m, const char* full_path, const char* archive_name) {
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity * 2;
        if (new_cap > MAX_MANIFEST_ENTRIES) return -1;
        ForensicManifestEntry* new_entries = (ForensicManifestEntry*)realloc(
            m->entries, new_cap * sizeof(ForensicManifestEntry));
        if (!new_entries) return -1;
        m->entries = new_entries;
        m->capacity = new_cap;
    }

    ForensicManifestEntry* e = &m->entries[m->count];
    memset(e, 0, sizeof(ForensicManifestEntry));

    e->archive_name = strdup(archive_name);
    e->original_path = strdup(full_path);

    /* Compute SHA-256 */
    compute_sha256(full_path, e->sha256);

    /* Stat the file */
    struct STAT st;
    if (STAT(full_path, &st) == 0) {
        e->size = st.st_size;
        e->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;

        unix_to_iso8601(st.st_mtime, e->mtime_iso, sizeof(e->mtime_iso));
        unix_to_iso8601(st.st_atime, e->atime_iso, sizeof(e->atime_iso));

#if defined(__APPLE__)
        unix_to_iso8601(st.st_birthtimespec.tv_sec, e->ctime_iso, sizeof(e->ctime_iso));
#elif defined(__linux__)
        unix_to_iso8601(st.st_ctime, e->ctime_iso, sizeof(e->ctime_iso));
#else
        e->ctime_iso[0] = '\0';
#endif

#ifndef _WIN32
        e->uid = (uint32_t)st.st_uid;
        e->gid = (uint32_t)st.st_gid;
        e->mode = (uint16_t)(st.st_mode & 07777);

        /* Get ACL */
        e->acl_text = get_acl_text(full_path);

        /* Get extended attributes */
        get_xattrs(full_path, &e->xattr_names, &e->xattr_values, &e->xattr_count);
#endif
    }

    m->count++;
    return 0;
}

/* Write manifest to a JSON file */
static int manifest_write_json(const ForensicManifest* m, const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) return -1;

    fprintf(f, "{\n");
    fprintf(f, "  \"forensic_manifest_version\": \"1.0\",\n");

    /* Tool info */
    char* esc_tool = json_escape(m->tool_name);
    char* esc_ver = json_escape(m->tool_version);
    char* esc_label = json_escape(m->source_label);
    fprintf(f, "  \"tool\": {\n");
    fprintf(f, "    \"name\": \"%s\",\n", esc_tool);
    fprintf(f, "    \"version\": \"%s\"\n", esc_ver);
    fprintf(f, "  },\n");
    fprintf(f, "  \"source_label\": \"%s\",\n", esc_label);
    fprintf(f, "  \"created\": \"%s\",\n", m->created_iso);
    fprintf(f, "  \"file_count\": %zu,\n", m->count);
    free(esc_tool);
    free(esc_ver);
    free(esc_label);

    /* Host info */
#ifndef _WIN32
    char hostname[256] = "";
    gethostname(hostname, sizeof(hostname));
    char* esc_host = json_escape(hostname);
    struct passwd* pw = getpwuid(getuid());
    char* esc_user = json_escape(pw ? pw->pw_name : "unknown");
    fprintf(f, "  \"acquisition_host\": {\n");
    fprintf(f, "    \"hostname\": \"%s\",\n", esc_host);
    fprintf(f, "    \"user\": \"%s\",\n", esc_user);
#if defined(__APPLE__)
    fprintf(f, "    \"platform\": \"macOS\"\n");
#elif defined(__linux__)
    fprintf(f, "    \"platform\": \"Linux\"\n");
#else
    fprintf(f, "    \"platform\": \"Unix\"\n");
#endif
    fprintf(f, "  },\n");
    free(esc_host);
    free(esc_user);
#endif

    /* Files array */
    fprintf(f, "  \"files\": [\n");
    for (size_t i = 0; i < m->count; i++) {
        const ForensicManifestEntry* e = &m->entries[i];
        char* esc_name = json_escape(e->archive_name);
        char* esc_path = json_escape(e->original_path);

        fprintf(f, "    {\n");
        fprintf(f, "      \"archive_name\": \"%s\",\n", esc_name);
        fprintf(f, "      \"original_path\": \"%s\",\n", esc_path);
        fprintf(f, "      \"sha256\": \"%s\",\n", e->sha256);
        fprintf(f, "      \"size\": %llu,\n", (unsigned long long)e->size);
        fprintf(f, "      \"timestamps\": {\n");
        fprintf(f, "        \"modified\": \"%s\",\n", e->mtime_iso);
        fprintf(f, "        \"created\": \"%s\",\n", e->ctime_iso);
        fprintf(f, "        \"accessed\": \"%s\"\n", e->atime_iso);
        fprintf(f, "      },\n");

#ifndef _WIN32
        fprintf(f, "      \"permissions\": {\n");
        fprintf(f, "        \"uid\": %u,\n", e->uid);
        fprintf(f, "        \"gid\": %u,\n", e->gid);
        fprintf(f, "        \"mode\": \"0%o\"\n", e->mode);
        fprintf(f, "      },\n");

        /* ACL */
        if (e->acl_text) {
            char* esc_acl = json_escape(e->acl_text);
            fprintf(f, "      \"acl\": \"%s\",\n", esc_acl);
            free(esc_acl);
        }

        /* Extended attributes */
        if (e->xattr_count > 0) {
            fprintf(f, "      \"xattrs\": {\n");
            for (size_t j = 0; j < e->xattr_count; j++) {
                char* esc_xname = json_escape(e->xattr_names[j]);
                char* esc_xval = json_escape(e->xattr_values[j]);
                fprintf(f, "        \"%s\": \"%s\"%s\n",
                        esc_xname, esc_xval,
                        j < e->xattr_count - 1 ? "," : "");
                free(esc_xname);
                free(esc_xval);
            }
            fprintf(f, "      },\n");
        }
#endif

        fprintf(f, "      \"is_directory\": %s\n", e->is_dir ? "true" : "false");
        fprintf(f, "    }%s\n", i < m->count - 1 ? "," : "");

        free(esc_name);
        free(esc_path);
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

/* ---- Public API ---- */

/**
 * Generate a forensic manifest JSON file for a set of input files/directories.
 *
 * The manifest contains per-file SHA-256 hashes, original paths, all timestamps,
 * Unix permissions, ACLs, and extended attributes.
 *
 * @param output_path   Path where the manifest JSON will be written
 * @param input_paths   NULL-terminated array of file/directory paths
 * @param source_label  Source provenance label (e.g., "Case 2024-001 Evidence")
 * @param progress_callback  Optional progress callback (bytes processed)
 * @param user_data     User data for progress callback
 * @return SEVENZIP_OK on success, error code on failure
 */
SevenZipErrorCode sevenzip_generate_forensic_manifest(
    const char* output_path,
    const char** input_paths,
    const char* source_label,
    SevenZipBytesProgressCallback progress_callback,
    void* user_data
) {
    if (!output_path || !input_paths) {
        return SEVENZIP_ERROR_INVALID_PARAM;
    }

    ForensicManifest* manifest = manifest_create(source_label);
    if (!manifest) return SEVENZIP_ERROR_MEMORY;

    /* First pass: count total bytes for progress */
    uint64_t total_bytes = 0;
    uint64_t processed_bytes = 0;

    for (int i = 0; input_paths[i] != NULL; i++) {
        struct STAT st;
        if (STAT(input_paths[i], &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                total_bytes += st.st_size;
            } else if (S_ISDIR(st.st_mode)) {
                /* Estimate — actual count done during recursion */
                total_bytes += st.st_size;
            }
        }
    }

    /* Process each input path */
    for (int i = 0; input_paths[i] != NULL; i++) {
        struct STAT st;
        if (STAT(input_paths[i], &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            const char* basename = strrchr(input_paths[i], '/');
#ifdef _WIN32
            if (!basename) basename = strrchr(input_paths[i], '\\');
#endif
            basename = basename ? basename + 1 : input_paths[i];

            manifest_add_file(manifest, input_paths[i], basename);
            processed_bytes += st.st_size;

            if (progress_callback) {
                progress_callback(processed_bytes, total_bytes, st.st_size, st.st_size,
                                  basename, user_data);
            }
        } else if (S_ISDIR(st.st_mode)) {
            /* Recursive directory traversal */
#ifndef _WIN32
            /* Use simple recursive approach */
            /* We need a stack-based traversal similar to multivolume */
            char** dir_stack = (char**)malloc(256 * sizeof(char*));
            size_t stack_cap = 256;
            size_t stack_size = 0;

            const char* base_dir_name = strrchr(input_paths[i], '/');
            base_dir_name = base_dir_name ? base_dir_name + 1 : input_paths[i];
            size_t base_path_len = strlen(input_paths[i]);

            dir_stack[stack_size++] = strdup(input_paths[i]);

            while (stack_size > 0) {
                char* current_dir = dir_stack[--stack_size];
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
                            if (stack_size >= stack_cap) {
                                stack_cap *= 2;
                                dir_stack = (char**)realloc(dir_stack, stack_cap * sizeof(char*));
                            }
                            dir_stack[stack_size++] = strdup(full_path);
                        } else if (S_ISREG(child_st.st_mode)) {
                            char relative_name[PATH_MAX];
                            snprintf(relative_name, PATH_MAX, "%s%s", base_dir_name, full_path + base_path_len);

                            manifest_add_file(manifest, full_path, relative_name);
                            processed_bytes += child_st.st_size;

                            if (progress_callback) {
                                progress_callback(processed_bytes, total_bytes,
                                                  child_st.st_size, child_st.st_size,
                                                  de->d_name, user_data);
                            }
                        }
                    }
                    closedir(dp);
                }
                free(current_dir);
            }
            free(dir_stack);
#endif
        }
    }

    /* Write manifest JSON */
    int result = manifest_write_json(manifest, output_path);
    manifest_free(manifest);

    return result == 0 ? SEVENZIP_OK : SEVENZIP_ERROR_COMPRESS;
}
