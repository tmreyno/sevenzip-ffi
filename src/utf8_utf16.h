/**
 * UTF-8 to UTF-16LE Conversion Utilities for 7z Archive Creation
 * 
 * The 7z format stores filenames as UTF-16LE (wchar_t on Windows).
 * This module provides correct multi-byte UTF-8 → UTF-16LE conversion,
 * including surrogate pair handling for codepoints above U+FFFF.
 * 
 * Previously, archive_create.c and archive_create_multivolume.c used
 * ASCII-only conversion (each byte → 1 UTF-16LE codeunit with 0x00 high byte),
 * which corrupted any filename containing non-ASCII characters (accents, CJK, emoji).
 */

#ifndef UTF8_UTF16_H
#define UTF8_UTF16_H

#include <stddef.h>
#include <stdint.h>

/**
 * Calculate the number of bytes needed to encode a UTF-8 string as UTF-16LE.
 * Includes the null terminator (2 bytes).
 * 
 * @param utf8  Null-terminated UTF-8 string
 * @return      Number of bytes for the UTF-16LE representation (including 2-byte null terminator)
 */
static inline size_t utf8_to_utf16le_size(const char* utf8) {
    size_t bytes = 0;
    const unsigned char* s = (const unsigned char*)utf8;
    
    while (*s) {
        uint32_t codepoint;
        
        if (*s < 0x80) {
            /* ASCII: 1 byte → 1 UTF-16LE code unit (2 bytes) */
            s++;
            bytes += 2;
        }
        else if ((*s & 0xE0) == 0xC0) {
            /* 2-byte UTF-8 (U+0080..U+07FF) → 1 UTF-16LE code unit (2 bytes) */
            if (s[1] == 0) break;  /* Truncated sequence */
            s += 2;
            bytes += 2;
        }
        else if ((*s & 0xF0) == 0xE0) {
            /* 3-byte UTF-8 (U+0800..U+FFFF) → 1 UTF-16LE code unit (2 bytes) */
            if (s[1] == 0 || s[2] == 0) break;  /* Truncated sequence */
            s += 3;
            bytes += 2;
        }
        else if ((*s & 0xF8) == 0xF0) {
            /* 4-byte UTF-8 (U+10000..U+10FFFF) → 2 UTF-16LE code units / surrogate pair (4 bytes) */
            if (s[1] == 0 || s[2] == 0 || s[3] == 0) break;  /* Truncated sequence */
            s += 4;
            bytes += 4;
        }
        else {
            /* Invalid UTF-8 byte — skip it, emit replacement character U+FFFD (2 bytes) */
            s++;
            bytes += 2;
        }
    }
    
    bytes += 2;  /* Null terminator */
    return bytes;
}

/**
 * Convert a UTF-8 string to UTF-16LE and write it to a buffer.
 * Writes the null terminator (0x00 0x00) at the end.
 * 
 * @param dst   Destination buffer (must be large enough — use utf8_to_utf16le_size())
 * @param utf8  Null-terminated UTF-8 string
 * @return      Number of bytes written (including null terminator)
 */
static inline size_t utf8_to_utf16le(unsigned char* dst, const char* utf8) {
    unsigned char* p = dst;
    const unsigned char* s = (const unsigned char*)utf8;
    
    while (*s) {
        uint32_t codepoint;
        
        if (*s < 0x80) {
            /* ASCII: 1 byte */
            codepoint = *s++;
        }
        else if ((*s & 0xE0) == 0xC0) {
            /* 2-byte UTF-8: 110xxxxx 10xxxxxx */
            if (s[1] == 0) break;
            codepoint = ((uint32_t)(s[0] & 0x1F) << 6)
                      | ((uint32_t)(s[1] & 0x3F));
            s += 2;
        }
        else if ((*s & 0xF0) == 0xE0) {
            /* 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx */
            if (s[1] == 0 || s[2] == 0) break;
            codepoint = ((uint32_t)(s[0] & 0x0F) << 12)
                      | ((uint32_t)(s[1] & 0x3F) << 6)
                      | ((uint32_t)(s[2] & 0x3F));
            s += 3;
        }
        else if ((*s & 0xF8) == 0xF0) {
            /* 4-byte UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
            if (s[1] == 0 || s[2] == 0 || s[3] == 0) break;
            codepoint = ((uint32_t)(s[0] & 0x07) << 18)
                      | ((uint32_t)(s[1] & 0x3F) << 12)
                      | ((uint32_t)(s[2] & 0x3F) << 6)
                      | ((uint32_t)(s[3] & 0x3F));
            s += 4;
        }
        else {
            /* Invalid UTF-8 byte — emit U+FFFD replacement character */
            codepoint = 0xFFFD;
            s++;
        }
        
        if (codepoint <= 0xFFFF) {
            /* Basic Multilingual Plane: single UTF-16LE code unit */
            *p++ = (unsigned char)(codepoint & 0xFF);
            *p++ = (unsigned char)((codepoint >> 8) & 0xFF);
        }
        else if (codepoint <= 0x10FFFF) {
            /* Supplementary plane: surrogate pair */
            uint32_t adj = codepoint - 0x10000;
            uint16_t high = 0xD800 + (uint16_t)(adj >> 10);
            uint16_t low  = 0xDC00 + (uint16_t)(adj & 0x3FF);
            *p++ = (unsigned char)(high & 0xFF);
            *p++ = (unsigned char)((high >> 8) & 0xFF);
            *p++ = (unsigned char)(low & 0xFF);
            *p++ = (unsigned char)((low >> 8) & 0xFF);
        }
        else {
            /* Invalid codepoint — emit U+FFFD */
            *p++ = 0xFD;
            *p++ = 0xFF;
        }
    }
    
    /* Null terminator */
    *p++ = 0;
    *p++ = 0;
    
    return (size_t)(p - dst);
}

#endif /* UTF8_UTF16_H */
