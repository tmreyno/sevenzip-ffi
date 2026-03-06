# sevenzip-ffi (C + Rust)

[![CI](https://github.com/tmreyno/sevenzip-ffi/actions/workflows/ci.yml/badge.svg)](https://github.com/tmreyno/sevenzip-ffi/actions/workflows/ci.yml)
[![Release](https://github.com/tmreyno/sevenzip-ffi/actions/workflows/release.yml/badge.svg)](https://github.com/tmreyno/sevenzip-ffi/actions/workflows/release.yml)
[![License: MIT OR Apache-2.0](https://img.shields.io/badge/license-MIT%20OR%20Apache--2.0-blue.svg)](LICENSE)

A SDK for 7z archive operations with both C and Rust interfaces, using the official LZMA SDK 23.01.

## ⚠️ Important: Large File Compression

**For files larger than 1GB, use `create_archive_streaming()` instead of `create_archive()`!**

The standard `create_archive()` loads entire files into memory, which will cause system failure with large inputs. The streaming API uses 64MB chunks and is safe for any file size.

```rust
// ❌ UNSAFE for large files - loads everything into RAM
sz.create_archive("output.7z", &["/path/to/large/folder"], level, None)?;

//  SAFE for any size - uses 64MB streaming chunks  
let mut opts = StreamOptions::default();
opts.chunk_size = 64 * 1024 * 1024;  // 64MB chunks
sz.create_archive_streaming("output.7z", &["/path/to/large/folder"], level, Some(&opts), None)?;
```

📖 See [docs/guides/MEMORY_SAFETY.md](docs/guides/MEMORY_SAFETY.md) for detailed guidance.

## Features

### Core Features

- **Extract 7z archives** - Compatible with 7-Zip format
- **List archive contents** - Inspect files with metadata (size, timestamp, CRC)
- **Create 7z archives** - Compatible with official 7-Zip
- **Split/Multi-volume archives** - Create and extract .7z.001, .7z.002, etc.
- **LZMA2 compression** - Modern compression with multi-threading support
- **LZMA2 decompression** - Fast decompression
- **Multi-file archives** - Multiple files in single .7z archive
- **Progress callbacks** - Track extraction/compression progress
- **C API** - Simple FFI interface
- **Rust crate** - Safe Rust bindings
- **Cross-platform** - Windows, macOS, Linux

### Advanced Features

- **Directory support** - Recursive directory archiving with empty directory preservation
- **Multi-threaded compression** - Configurable thread count
- **Custom compression options** - Control thread count, dictionary size, solid mode
- **Streaming compression** - Process files larger than RAM with chunk-based streaming
- **Split/multi-volume archives** - Create and extract split archives (4GB, 8GB, custom sizes)
- **Large file support** - Large files with streaming
- **AES-256 encryption** - Pure Rust implementation (no OpenSSL required)

See [docs/guides/ADVANCED_FEATURES.md](docs/guides/ADVANCED_FEATURES.md) for detailed documentation.

## Quick Start

### Using the Rust Crate

Add to your `Cargo.toml`:

```toml
[dependencies]
seven-zip = { path = "path/to/sevenzip-ffi/rust" }
```

Example:

```rust
use seven_zip::{SevenZip, CompressionLevel};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let sz = SevenZip::new()?;
    
    // Extract archive
    sz.extract("archive.7z", "output/")?;
    
    // List contents
    let entries = sz.list("archive.7z", None)?;
    for entry in entries {
        println!("{}: {} bytes", entry.name, entry.size);
    }
    
    // Create archive
    sz.create_archive("backup.7z", &["file1.txt", "dir/"], CompressionLevel::Normal, None)?;
    
    Ok(())
}
```

### Pure Rust Encryption (No OpenSSL!)

```rust
use seven_zip::encryption_native::EncryptionContext;

// Encrypt data
let ctx = EncryptionContext::new("strong_password")?;
let ciphertext = ctx.encrypt(b"Secret data")?;

// Decrypt data  
let plaintext = ctx.decrypt(&ciphertext)?;
```

See [docs/guides/RUST_README.md](docs/guides/RUST_README.md) for complete Rust documentation.

### Using the C API

## Building

### Prerequisites

- CMake 3.15+
- C/C++ compiler (GCC, Clang, or MSVC)
- 7z SDK (LZMA SDK) - included in the `lzma` directory

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Usage in Tauri

1. Build the library as shown above
2. Copy the resulting library to your Tauri project's resources
3. Use Tauri's FFI capabilities to call the functions

### Example Tauri Integration

```rust
// In your Tauri Rust code
use tauri::api::process::Command;

// Load and use the FFI library
```

## API Reference

See `include/7z_ffi.h` for the complete API documentation.

### Main Functions

- `sevenzip_extract()` - Extract an archive
- `sevenzip_compress()` - Create an archive
- `sevenzip_list()` - List archive contents
- `sevenzip_create_7z_streaming()` - **NEW!** Streaming compression for large files
- `sevenzip_extract_streaming()` - **NEW!** Extract split/multi-volume archives
- `sevenzip_free()` - Free allocated memory

## Documentation

- **[docs/guides/QUICKSTART.md](docs/guides/QUICKSTART.md)** - Getting started guide
- **[docs/guides/ADVANCED_FEATURES.md](docs/guides/ADVANCED_FEATURES.md)** - Multi-threading, encryption, split archives
- **[docs/guides/LARGE_FILES_GUIDE.md](docs/guides/LARGE_FILES_GUIDE.md)** - 100GB+ file compression
- **[docs/guides/MEMORY_SAFETY.md](docs/guides/MEMORY_SAFETY.md)** - Memory-safe streaming API
- **[docs/guides/RUST_README.md](docs/guides/RUST_README.md)** - Rust crate documentation
- **[docs/guides/BUILD_AND_USAGE.md](docs/guides/BUILD_AND_USAGE.md)** - Build and usage instructions
- **[DOCUMENTATION.md](DOCUMENTATION.md)** - Full documentation index
- **[CHANGELOG.md](CHANGELOG.md)** - Release history

## License

This wrapper is provided as-is. The LZMA SDK is in the public domain.
