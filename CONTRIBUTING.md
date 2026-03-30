# Contributing to sevenzip-ffi

Thank you for your interest in contributing to sevenzip-ffi!

## Prerequisites

- **C compiler** — GCC, Clang, or MSVC
- **CMake** ≥ 3.14
- **Rust** ≥ 1.70 (for Rust FFI bindings)
- **Node.js** ≥ 16 (for N-API bindings, optional)

## Development Setup

```bash
git clone https://github.com/tmreyno/sevenzip-ffi.git
cd sevenzip-ffi

# Build C library
mkdir -p build && cd build
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)

# Run C tests
ctest

# Run Rust tests
cd ../rust
cargo test
```

## Code Style

- **C code**: Follow existing style in `src/`. Use `portable_aligned_alloc()` macros instead of C11 `aligned_alloc()`.
- **Rust code**: Run `cargo fmt` and `cargo clippy` before committing.
- **Filenames**: Use `utf8_to_utf16le()` from `utf8_utf16.h` for all 7z filename encoding — never ASCII-only loops.

## Pull Requests

1. Fork the repository and create a feature branch
2. Make your changes with clear commit messages
3. Ensure all tests pass (`ctest` + `cargo test`)
4. Submit a pull request with a description of the changes

## Reporting Issues

Open an issue on GitHub with:
- Platform and compiler version
- Steps to reproduce
- Expected vs actual behavior

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
