# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-01-30

### Added
- Initial release of `sevenzip-ffi` C + Rust SDK
- C FFI library (`lib7z_ffi`) for 7z archive operations
- Rust crate (`seven-zip`) with safe bindings over the C library
- NAPI Node.js package (`@sevenzip/napi`) for JavaScript/TypeScript consumers
- Core archive operations: extract, list contents, create archives
- LZMA2 compression with multi-threading support
- Multi-volume/split archive support (`.7z.001`, `.7z.002`, …)
- Streaming compression API (`create_archive_streaming`) for files larger than RAM
- AES-256 encryption support (pure Rust, no OpenSSL required)
- Progress callback support for long-running operations
- Cross-platform builds: Linux (x86_64/aarch64), macOS (x86_64/aarch64), Windows (x86_64/i686/aarch64)
- CI workflow with automated builds and tests on all platforms
- GitHub Actions release workflow for automated publishing
- Comprehensive documentation (`README.md`, `DOCUMENTATION.md`, `ADVANCED_FEATURES.md`, `MEMORY_SAFETY.md`)

### Fixed
- MSVC compatibility: use `_aligned_malloc` instead of C11 `aligned_alloc`

[Unreleased]: https://github.com/tmreyno/sevenzip-ffi/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/tmreyno/sevenzip-ffi/releases/tag/v1.0.0
