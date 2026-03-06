# Changelog

All notable changes to **sevenzip-ffi** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [v1.2.0] - 2026-01-31

### Added
- Pure Rust AES-256 encryption (`encryption_native.rs`) — no OpenSSL dependency
- Streaming compression API (`create_archive_streaming`) for files larger than RAM
- Multi-volume / split archive support (`.7z.001`, `.7z.002`, …)
- Large-file demos and examples (`large_file_demo`, `test_multivolume`)
- NAPI bindings for Node.js (`napi/`)
- Comprehensive documentation reorganised under `docs/` and `rust/docs/`
- Performance benchmarks under `rust/benches/`

### Changed
- Minimum supported Rust edition: 2021
- `SevenZip::new()` now validates the underlying C library at construction time
- Compression level enum extended with `Ultra` variant

### Fixed
- Directory traversal respects empty-directory preservation flag
- Windows path handling in C library FFI layer

## [v1.1.0] - 2025-11-15

### Added
- Multi-threaded LZMA2 compression with configurable thread count
- Custom compression options (`CompressionOptions` struct)
- Progress callback support for extract and compress operations
- Cross-platform CI workflow (Ubuntu, macOS, Windows)

### Changed
- Improved error messages returned from C layer via `error_reporting.c`
- Renamed `sevenzip_compress()` to `sevenzip_create_archive()` for clarity

### Fixed
- Memory leak in `archive_extract.c` when extraction fails mid-way
- CRC mismatch errors on certain 7z archives created by Windows 7-Zip

## [v1.0.0] - 2025-09-01

### Added
- Initial release
- Extract 7z archives (`sevenzip_extract`)
- Create 7z archives (`sevenzip_compress`)
- List archive contents (`sevenzip_list`)
- C FFI interface (`include/7z_ffi.h`)
- Safe Rust bindings (`rust/`)
- CMake build system
- LZMA SDK 23.01 bundled in `lzma/`

[Unreleased]: https://github.com/tmreyno/sevenzip-ffi/compare/v1.2.0...HEAD
[v1.2.0]: https://github.com/tmreyno/sevenzip-ffi/compare/v1.1.0...v1.2.0
[v1.1.0]: https://github.com/tmreyno/sevenzip-ffi/compare/v1.0.0...v1.1.0
[v1.0.0]: https://github.com/tmreyno/sevenzip-ffi/releases/tag/v1.0.0
