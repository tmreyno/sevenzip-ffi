# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in sevenzip-ffi, please report it responsibly:

1. **Do NOT** open a public GitHub issue
2. Email the maintainers or use GitHub's private security advisory feature

## Scope

sevenzip-ffi is an archive creation library. Security considerations include:

- **Memory safety** — Buffer overflows in C code, especially header building and filename encoding
- **Path traversal** — Archive entry paths must be validated by consuming applications
- **Integer overflow** — File size and offset calculations with large archives
- **Compression bombs** — Consuming applications should validate decompressed sizes

## Supported Versions

Only the latest release receives security updates.

## Dependencies

- **LZMA SDK 24.09** — vendored C source in `lzma/C/`
- No network dependencies — all operations are local file I/O
