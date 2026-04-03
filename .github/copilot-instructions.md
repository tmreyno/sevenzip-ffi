# sevenzip-ffi — Copilot Instructions

You are working on **sevenzip-ffi**, a C library + Rust FFI wrapper for 7z archive operations using **LZMA SDK 24.09**.

## Architecture

```text
src/                          # C source files (21 files)
  ├── archive_create.c        # Single-volume 7z archive creation
  ├── archive_create_multivolume.c  # Split/multi-volume 7z creation
  ├── archive_create_custom.c # Custom compression options
  ├── archive_create_true_streaming.c  # Chunk-based streaming for large files
  ├── archive_extract.c       # Standard extraction
  ├── archive_extract_custom.c # Custom extraction
  ├── archive_extract_split.c # Split archive extraction
  ├── archive_list.c          # Archive content listing
  ├── archive_repair.c        # Archive repair/recovery
  ├── archive_stream_api.c    # Streaming API helpers
  ├── archive_test.c          # Archive integrity testing
  ├── buffer_compression.c    # In-memory LZMA2 compression
  ├── encryption_aes.c/.h     # AES-256 encryption (pure C, no OpenSSL)
  ├── error_reporting.c       # Error handling utilities
  ├── ffi_interface.c         # Main FFI entry points
  ├── filters.c               # Data filters
  ├── forensic_manifest.c     # Forensic hash manifest generation
  ├── lzma_compress.c         # LZMA2 compression
  ├── lzma_decompress.c       # LZMA2 decompression
  └── utf8_utf16.h            # UTF-8 → UTF-16LE filename encoding

include/7z_ffi.h              # Public C API header
lzma/C/                       # LZMA SDK 24.09 C source (86 files)
rust/                         # Safe Rust bindings crate (seven-zip)
napi/                         # Node.js N-API bindings (@sevenzip/napi)
build/                        # CMake build output
prebuilt/                     # Pre-built static libraries for CI
  ├── macos-arm64/lib7z_ffi.a
  ├── linux-x64/lib7z_ffi.a
  └── windows-x64-msvc/7z_ffi.lib
```

## Critical Invariants

### 1. UTF-8 → UTF-16LE Filenames

7z format stores filenames as UTF-16LE. **ALL** filename encoding MUST use `utf8_to_utf16le()` and `utf8_to_utf16le_size()` from `src/utf8_utf16.h`. **NEVER** use the ASCII-only loop (`*p++ = (Byte)*name++; *p++ = 0;`) — it corrupts non-ASCII filenames (CJK, emoji, accented chars).

### 2. Dictionary Sizes (SDK 24.09 Defaults)

When `dict_size = 0`, these defaults apply:

| Level | Name | Dict Size |
|-------|------|-----------|
| 0 | STORE | 64 KB (`1 << 16`) |
| 1 | FASTEST | 256 KB (`1 << 18`) |
| 3 | FAST | 4 MB (`1 << 22`) |
| 5 | NORMAL | 32 MB (`1 << 25`) |
| 7 | MAXIMUM | 128 MB (`1 << 27`) |
| 9 | ULTRA | 256 MB (`1 << 28`) |

The multivolume creator lets `Lzma2EncProps_Normalize()` set dictionary from level (correct — uses SDK defaults automatically).

### 3. Dynamic Header Allocation

`build_7z_header()` uses `calc_7z_header_size()` for pre-allocation + `CHECK_SPACE()` macro for safety. The old 256KB fixed buffer caused heap overflow with >625 files. **NEVER replace dynamic allocation with a fixed buffer.**

### 4. Entropy Threshold

Both `archive_create.c` and `archive_create_multivolume.c` use `unique_bytes < 220` for compressibility detection. Keep them in sync.

### 5. SDK Version

LZMA SDK files in `lzma/C/` are version **24.09**. SDK 24.09 auto-detects `MY_CPU_ARM64` from `__aarch64__` — do NOT add `MY_CPU_ARM64` to CMake compile definitions.

### 6. Memory Safety — Large Files

For files >1GB, use `create_archive_streaming()` instead of `create_archive()`. The standard API loads entire files into memory; the streaming API uses 64MB chunks.

### 7. Windows Compatibility

Use `portable_aligned_alloc()` / `portable_aligned_free()` macros instead of C11 `aligned_alloc()` — MSVC does not provide `aligned_alloc`. The macros map to `_aligned_malloc()` / `_aligned_free()` on Windows.

### 8. Static Libraries Are Canonical

This repo's default CMake output must remain a static library. CI, release artifacts, Rust bindings, and CORE consumers all expect `build/lib7z_ffi.a` on Unix-like platforms and `build/Release/7z_ffi.lib` on Windows. Shared-library builds are opt-in via `-DBUILD_SHARED_LIBS=ON`.

## Build & Test

```bash
# Build (macOS/Linux)
mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)

# Test Rust bindings
cargo test --lib --tests

# Deploy to CORE-1
cp build/lib7z_ffi.a /path/to/CORE-1/sevenzip-ffi/build/lib7z_ffi.a
```

## Consumer: CORE-FFX

This library is consumed by **CORE-FFX** (CORE-1 repo) for forensic 7z archive creation. The CORE-1 repo contains a copy of the pre-built static library at `sevenzip-ffi/build/lib7z_ffi.a` (macOS) and `sevenzip-ffi/prebuilt/` (all platforms).

## CI/CD

- `.github/workflows/ci.yml` — Build + test on Linux, macOS, Windows
- `.github/workflows/release.yml` — Tag push (`v*`) triggers release build + npm publish
- Workflow LZMA bootstrap must call `scripts/setup_lzma.sh` from the repo root; do not point CI at a nonexistent top-level `setup_lzma.sh`
- `scripts/setup_lzma.sh` must no-op when the vendored `lzma/C` tree is already present; CI should not depend on reaching `7-zip.org`
- Rust toolchain setup in workflows must use `dtolnay/rust-toolchain@stable`
- Linux workflow jobs that compile `forensic_manifest.c` must install `libacl1-dev` so `sys/acl.h` is available
- `CMakeLists.txt` must link `acl` on Linux because `forensic_manifest.c` uses POSIX ACL APIs
- Unix workflow configure steps must pass `-DBUILD_SHARED_LIBS=OFF` so fresh CI checkouts produce the static library expected by the Rust bindings
- `rust/build.rs` must request `-DBUILD_SHARED_LIBS=OFF` when it bootstraps the C library itself
- Both Rust `build.rs` files must link `acl` on Linux when statically linking `7z_ffi`
- Windows CI and release builds should configure CMake with `BUILD_EXAMPLES=OFF` and `BUILD_TESTS=OFF`; the library build is required, but the bundled C examples/tests are not Windows-portable today

## Do NOT

- Use the ASCII-only encoding loop for filenames — use `utf8_to_utf16le()` from `utf8_utf16.h`
- Use hardcoded dictionary sizes that don't match SDK 24.09 defaults
- Use a fixed-size buffer for `build_7z_header()` — dynamic allocation is required
- Add `MY_CPU_ARM64` to CMakeLists.txt — SDK 24.09 detects it automatically
- Change the entropy threshold (220) in one file without updating the other
- Use C11 `aligned_alloc()` in C code — use `portable_aligned_alloc()` macro
- Downgrade the LZMA SDK from 24.09 to an older version
- Point GitHub Actions at a top-level `setup_lzma.sh` — the bootstrap script lives under `scripts/`
- Force CI to download the SDK when `lzma/C` is already vendored — the setup script must short-circuit offline
- Use `dtolnay/rust-action` in workflows — the valid action is `dtolnay/rust-toolchain@stable`
- Omit `libacl1-dev` from Linux workflow jobs — `forensic_manifest.c` includes `sys/acl.h`
- Remove Linux `acl` linkage from `CMakeLists.txt` while `forensic_manifest.c` still calls POSIX ACL APIs
- Change `BUILD_SHARED_LIBS` back to `ON` by default — the Rust bindings and shipped artifacts are static-first
- Let Unix CI configure CMake without `-DBUILD_SHARED_LIBS=OFF` — fresh checkouts will only produce shared objects and the Rust bindings will fail to link
- Remove Linux `acl` linking from either Rust `build.rs` while static linking remains the default
- Require Windows CI to build the bundled examples/tests until they are made Windows-portable
