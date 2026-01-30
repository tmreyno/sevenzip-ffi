# 7z FFI SDK - Build and Usage Guide

## Built Libraries

### C Library

- **Location**: `build/lib7z_ffi.a`
- **Size**: ~312 KB (static library)
- **Format**: Static library for macOS ARM64
- **Dependencies**: pthread

### Rust Crate

- **Location**: `rust/target/release/libseven_zip.rlib`
- **Size**: ~546 KB
- **Format**: Rust library
- **Version**: 1.3.0

## Using the C Library

### 1. Link in Your C/C++ Project

```c
// Include the header
#include "include/7z_ffi.h"

// Compile and link
gcc your_program.c -I./include -L./build -l7z_ffi -lcrypto -lpthread -o your_program
```

### 2. Basic Usage Example

```c
#include "7z_ffi.h"
#include <stdio.h>

int main() {
    // Initialize library
    SevenZipErrorCode result = sevenzip_init();
    if (result != SEVENZIP_OK) {
        fprintf(stderr, "Failed to initialize\n");
        return 1;
    }
    
    // Create archive
    const char* files[] = {"file1.txt", "file2.txt", NULL};
    result = sevenzip_create_archive(
        "output.7z",
        files,
        SEVENZIP_LEVEL_NORMAL,
        "password123"  // Optional password
    );
    
    if (result == SEVENZIP_OK) {
        printf("Archive created successfully\n");
    }
    
    return 0;
}
```

## Using the Rust Crate

### 1. Add to Your Cargo.toml

#### Option A: From Local Path
```toml
[dependencies]
seven-zip = { path = "/path/to/7z-ffi-sdk/rust" }
```

#### Option B: Copy to Your Project
```bash
# Copy the rust directory to your project
cp -r /path/to/7z-ffi-sdk/rust ./seven-zip
cp -r /path/to/7z-ffi-sdk/build ./seven-zip/
```

Then in `Cargo.toml`:
```toml
[dependencies]
seven-zip = { path = "./seven-zip" }
```

### 2. Basic Usage Example

```rust
use seven_zip::{SevenZip, CompressionLevel, CompressOptions};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize library
    let sz = SevenZip::new()?;
    
    // Create archive
    sz.create_archive(
        "output.7z",
        &["file1.txt", "file2.txt"],
        CompressionLevel::Normal,
        None  // No password
    )?;
    
    // Create encrypted archive
    let options = CompressOptions {
        password: Some("password123".to_string()),
        ..Default::default()
    };
    
    sz.create_archive(
        "encrypted.7z",
        &["secret.txt"],
        CompressionLevel::Maximum,
        Some(&options)
    )?;
    
    // Extract archive
    sz.extract("output.7z", "extracted_files")?;
    
    // Extract with password
    sz.extract_with_password(
        "encrypted.7z",
        "extracted_secret",
        Some("password123"),
        None
    )?;
    
    // List archive contents
    let entries = sz.list("output.7z", None)?;
    for entry in entries {
        println!("{}: {} bytes", entry.name, entry.size);
    }
    
    // Test archive integrity
    sz.test_archive("output.7z", None)?;
    println!("Archive is valid!");
    
    Ok(())
}
```

### 3. Advanced Features

```rust
use seven_zip::{SevenZip, CompressionLevel, CompressOptions};

fn advanced_usage() -> Result<(), Box<dyn std::error::Error>> {
    let sz = SevenZip::new()?;
    
    // Multi-threaded compression with custom options
    let options = CompressOptions {
        num_threads: 8,          // Use 8 threads
        dict_size: 64 * 1024 * 1024,  // 64MB dictionary
        solid: true,             // Solid archive
        password: Some("secure_password".to_string()),
    };
    
    sz.create_archive(
        "large_archive.7z",
        &["large_file1.bin", "large_file2.bin"],
        CompressionLevel::Ultra,
        Some(&options)
    )?;
    
    // Extract specific files
    sz.extract_files(
        "large_archive.7z",
        "output_dir",
        &["large_file1.bin"],
        Some("secure_password")
    )?;
    
    Ok(())
}
```

## Running Examples

### C Examples
```bash
cd /path/to/7z-ffi-sdk

# Forensic archiver
./build/examples/forensic_archiver compress output.7z /path/to/files --password

# Test encryption
./build/examples/test_encryption

# Create 7z archive
./build/examples/example_create_7z
```

### Rust Examples
```bash
cd /path/to/7z-ffi-sdk/rust

# Feature demonstration (all features)
cargo run --release --example feature_demo

# Forensic archiver
cargo run --release --example forensic_archiver

# Encryption example
cargo run --release --example encryption_example

# Basic demo
cargo run --release --example demo
```

## Running Tests

### C Tests
```bash
cd /path/to/7z-ffi-sdk
./build/tests/test_compress
./build/tests/test_extract
```

### Rust Tests
```bash
cd /path/to/7z-ffi-sdk/rust

# All tests
cargo test --release

# Unit tests only
cargo test --release --lib

# Integration tests only
cargo test --release --test integration_tests
```

## Distribution

### Creating a Release Package

```bash
cd /path/to/7z-ffi-sdk

# Create distribution directory
mkdir -p dist/lib dist/include dist/examples

# Copy C library
cp build/lib7z_ffi.a dist/lib/

# Copy headers
cp include/7z_ffi.h dist/include/

# Copy examples
cp build/examples/forensic_archiver dist/examples/
cp build/examples/test_encryption dist/examples/

# Copy Rust crate (entire directory)
cp -r rust dist/

# Create archive
tar czf 7z-ffi-sdk-v1.2.0-macos-arm64.tar.gz dist/
```

### Using in Another Rust Project

1. **Copy the crate**:
   ```bash
   cp -r /path/to/7z-ffi-sdk/rust /your/project/deps/seven-zip
   cp -r /path/to/7z-ffi-sdk/build /your/project/deps/seven-zip/
   ```

2. **Add to Cargo.toml**:
   ```toml
   [dependencies]
   seven-zip = { path = "deps/seven-zip" }
   ```

3. **Use in your code**:
   ```rust
   use seven_zip::{SevenZip, CompressionLevel};
   ```

## System Requirements

### Runtime Dependencies

- **pthread**: For thread-safe operations

### Build Dependencies

- **CMake**: 3.10 or higher
- **C Compiler**: GCC, Clang, or MSVC
- **Rust**: 1.70 or higher (for Rust crate)

### Platform Support

- ✅ macOS (ARM64 and x86_64)
- ✅ Linux (glibc-based)
- ✅ Windows (MSVC toolchain)

## Features

### Compression
- 5 compression levels (Store, Fast, Normal, Maximum, Ultra)
- LZMA2 compression algorithm
- Solid archives for maximum compression
- Multi-threaded compression

### Encryption
- AES-256-CBC encryption
- Password-based key derivation (PBKDF2)
- Thread-safe encryption contexts

### Archive Operations
- Create 7z archives
- Extract archives
- List archive contents
- Test archive integrity
- Extract specific files

### Error Handling
- Thread-safe error reporting
- Detailed error messages with context
- Actionable suggestions for common errors

## API Documentation

### C API
See `include/7z_ffi.h` for complete API documentation.

Key functions:
- `sevenzip_init()` - Initialize library
- `sevenzip_create_archive()` - Create archive
- `sevenzip_extract()` - Extract archive
- `sevenzip_list_archive()` - List contents
- `sevenzip_test_archive()` - Test integrity

### Rust API
Run `cargo doc --open` in the `rust/` directory for complete documentation.

Key types:
- `SevenZip` - Main archive interface
- `CompressionLevel` - Compression level enum
- `CompressOptions` - Advanced compression options
- `ArchiveEntry` - File entry information

## Performance Notes

### Compression Speed
- **Store**: Instant (no compression)
- **Fast**: 10-20 MB/s
- **Normal**: 5-10 MB/s
- **Maximum**: 2-5 MB/s
- **Ultra**: 0.5-2 MB/s (best compression)

### Memory Usage
- **Dictionary Size**: 16MB default, up to 1536MB
- **Multi-threading**: ~100MB per thread
- **Typical**: 200-500MB for normal compression

### Compression Ratio
- **Text files**: 70-90% reduction
- **Binary data**: 40-60% reduction
- **Pre-compressed**: 0-10% reduction

## Troubleshooting

### Build Issues

**Problem**: CMake not found
```bash
# macOS
brew install cmake

# Linux
sudo apt-get install cmake
```

**Problem**: OpenSSL not found
```bash
# macOS
brew install openssl@3

# Linux
sudo apt-get install libssl-dev
```

### Runtime Issues

**Problem**: Library not found
```bash
# macOS - Add to library path
export DYLD_LIBRARY_PATH=/opt/homebrew/lib:$DYLD_LIBRARY_PATH

# Linux
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

**Problem**: Rust linking errors
```bash
# Clean and rebuild
cd rust
cargo clean
cd ..
rm -rf build
cmake -B build && cmake --build build
cd rust
cargo build --release
```

## License

See LICENSE file for details.

## Support

For issues and questions, refer to:
- C API: `include/7z_ffi.h`
- Rust API: `cargo doc --open` in rust directory
- Examples: `examples/` directory
- Tests: `tests/` directory (C), `rust/tests/` (Rust)
