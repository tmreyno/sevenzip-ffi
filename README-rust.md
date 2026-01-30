# seven-zip

A Rust wrapper for 7z archive operations using LZMA2 compression.

This crate provides a high-level Rust API for working with 7z archives, built on top of the official LZMA SDK.

## Features

- **Extract 7z archives** - Extraction with format support
- **Create 7z archives** - Compatible with official 7-Zip
- **List archive contents** - Get file information without extraction
- **Split/multi-volume archives** - Create and extract .7z.001, .7z.002, etc.
- **AES-256 encryption** - Pure Rust (no OpenSSL required)
- **LZMA2 compression** - Modern compression algorithm
- **Multi-file archives** - Multiple files in single archive
- **Cross-platform** - Works on macOS, Linux, and Windows
- **Safe API** - Memory-safe Rust interface with error handling
- **Progress tracking** - Optional callbacks for long operations

## Installation

Add this to your `Cargo.toml`:

```toml
[dependencies]
seven-zip = { path = "path/to/sevenzip-ffi/rust" }
```

### Build Requirements

- CMake 3.15 or higher
- C11 compiler (gcc, clang, or MSVC)

The crate includes the LZMA SDK and will build it automatically.

## Quick Start

### Extract a 7z archive

```rust
use seven_zip::SevenZip;

let sz = SevenZip::new()?;
sz.extract("archive.7z", "output/")?;
```

### Create a 7z archive (7-Zip compatible!)

```rust
use seven_zip::{SevenZip, CompressionLevel};

let sz = SevenZip::new()?;
sz.create_archive(
    "backup.7z",
    &["file1.txt", "file2.txt", "directory/"],
    CompressionLevel::Normal,
    None
)?;
```

### List archive contents

```rust
use seven_zip::SevenZip;

let sz = SevenZip::new()?;
let entries = sz.list("archive.7z", None)?;

for entry in entries {
    println!("{}: {} bytes", entry.name, entry.size);
}
```

### Pure Rust AES-256 Encryption

```rust
use seven_zip::encryption_native::EncryptionContext;

// Create encryption context (generates random salt/IV)
let ctx = EncryptionContext::new("strong_password")?;

// Encrypt data
let plaintext = b"Secret secret data";
let ciphertext = ctx.encrypt(plaintext)?;

// Decrypt data
let decrypted = ctx.decrypt(&ciphertext)?;
assert_eq!(plaintext.as_slice(), decrypted.as_slice());
```

### Decrypt with password and salt

```rust
use seven_zip::encryption_native::DecryptionContext;

// Salt and IV from encrypted archive header
let salt = /* from archive */;
let iv = /* from archive */;

let ctx = DecryptionContext::new("password", &salt)?;
let plaintext = ctx.decrypt(&ciphertext, &iv)?;
```

## Examples

The crate includes several complete examples:

```bash
# Extract an archive
cargo run --example extract -- archive.7z output/

# List archive contents
cargo run --example list -- archive.7z

# Compress a file
cargo run --example compress -- input.txt output.lzma2 maximum

# Create a multi-file archive
cargo run --example create_archive -- backup.7zff file1.txt file2.txt
```

## API Documentation

### `SevenZip`

Main interface for 7z operations.

```rust
pub struct SevenZip { /* ... */ }

impl SevenZip {
    pub fn new() -> Result<Self>;
    pub fn version(&self) -> String;
    pub fn extract<P: AsRef<Path>>(&self, archive_path: P, output_dir: P) -> Result<()>;
    pub fn list<P: AsRef<Path>>(&self, archive_path: P) -> Result<Vec<ArchiveEntry>>;
    pub fn compress_file<P: AsRef<Path>>(&self, input: P, output: P, level: CompressionLevel) -> Result<()>;
    pub fn decompress_file<P: AsRef<Path>>(&self, input: P, output: P) -> Result<()>;
    pub fn create_archive<P: AsRef<Path>>(&self, archive: P, files: &[P], level: CompressionLevel) -> Result<()>;
    pub fn extract_custom_archive<P: AsRef<Path>>(&self, archive: P, output: P) -> Result<()>;
}
```

### `CompressionLevel`

Controls compression speed vs. ratio tradeoff.

```rust
pub enum CompressionLevel {
    Store,    // No compression (fastest)
    Fastest,  // Minimal compression
    Fast,     // Fast compression
    Normal,   // Balanced (default)
    Maximum,  // Maximum compression
    Ultra,    // Ultra compression (slowest)
}
```

### `ArchiveEntry`

Information about a file in an archive.

```rust
pub struct ArchiveEntry {
    pub name: String,
    pub size: u64,
    pub packed_size: u64,
    pub modified: u64,
    pub attributes: u32,
    pub crc: u32,
    pub is_dir: bool,
}
```

### Error Handling

All operations return `Result<T, Error>`:

```rust
use seven_zip::{SevenZip, Error};

match sz.extract("archive.7z", "output/") {
    Ok(()) => println!("Success!"),
    Err(Error::InvalidArchive(msg)) => eprintln!("Invalid archive: {}", msg),
    Err(Error::OpenFile(msg)) => eprintln!("Cannot open file: {}", msg),
    Err(e) => eprintln!("Error: {}", e),
}
```

## Performance

The LZMA2 compression used by this crate typically achieves:

- **Text files**: 60-80% compression ratio
- **Source code**: 50-70% compression ratio
- **Binary executables**: 30-50% compression ratio
- **Already compressed**: 0-10% additional compression

Compression speed depends on the level:
- **Store**: Instant (no compression)
- **Fastest**: ~100 MB/s
- **Normal**: ~20-40 MB/s
- **Ultra**: ~5-10 MB/s (highest ratio)

Decompression is fast across all levels (~100-200 MB/s).

## Platform Support

This crate works on:

- **macOS** (x86_64, ARM64)
- **Linux** (x86_64, ARM64, ARM)
- **Windows** (x86_64, x86)

The build system automatically detects your platform and configures accordingly.

## Architecture

```
┌─────────────────────┐
│   Rust Application  │
└──────────┬──────────┘
           │ Safe API
┌──────────▼──────────┐
│  seven-zip crate    │
│  (Safe Rust)        │
└──────────┬──────────┘
           │ FFI
┌──────────▼──────────┐
│  sevenzip-ffi (C)     │
│  CMake Library      │
└──────────┬──────────┘
           │
┌──────────▼──────────┐
│  LZMA SDK 23.01     │
│  (60+ C files)      │
└─────────────────────┘
```

## Technical Details

### Formats

- **Extract**: Standard .7z format
- **Create**: Standard .7z archives
- **Multi-file**: Standard .7z format with solid compression
- **Split**: Multi-volume archives (.7z.001, .7z.002, etc.)
- **Encryption**: AES-256-CBC (pure Rust, no OpenSSL)

### Dependencies

The crate is **self-contained** - the LZMA SDK is compiled into the library. No runtime dependencies are needed.

**Build dependencies**:
- `cc` - C compiler interface
- `cmake` - CMake build system

**Runtime dependencies**:
- `libc` - C types for FFI
- `thiserror` - Error handling

### Thread Safety

The `SevenZip` struct is `Send + Sync`, allowing it to be used across threads safely. Each operation is independent and can be called concurrently.

## Use in Tauri

This crate is designed for Tauri applications:

```rust
use seven_zip::SevenZip;

#[tauri::command]
fn extract_archive(archive: String, output: String) -> Result<(), String> {
    let sz = SevenZip::new().map_err(|e| e.to_string())?;
    sz.extract(&archive, &output).map_err(|e| e.to_string())?;
    Ok(())
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![extract_archive])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

## Testing

Run tests:

```bash
cargo test
```

## License

This project includes the LZMA SDK which is in the public domain. The Rust wrapper code is MIT licensed.

## Links

- [GitHub Repository](https://github.com/tmreyno/sevenzip-ffi)
- [LZMA SDK](https://www.7-zip.org/sdk.html)
- [7-Zip](https://www.7-zip.org/)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Changelog

### 1.0.0 (January 2026)

- Initial release
- 7z extraction support
- LZMA2 compression/decompression
- Standard .7z multi-file archives
- Pure Rust AES-256 encryption (no OpenSSL)
- Safe Rust API with error handling
- Cross-platform support (macOS, Linux, Windows)
