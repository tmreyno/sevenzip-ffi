# sevenzip-ffi Documentation Index

Complete documentation for the sevenzip-ffi library - Rust bindings for 7-Zip with LZMA2 compression.

## 📚 Quick Start

- **[README.md](README.md)** - Project overview and quick start
- **[docs/guides/](docs/guides/)** - User guides and tutorials
- **[rust/README.md](rust/README.md)** - Rust library documentation

## 📖 User Guides

### Getting Started
Located in `docs/guides/`:
- **QUICKSTART.md** - Quick start guide (5 minutes)
- **SETUP.md** - Detailed setup instructions
- **BUILD_AND_USAGE.md** - Building and using the library
- **RUST_README.md** - Rust-specific quick start

### Advanced Features
Located in `docs/guides/`:
- **ADVANCED_FEATURES.md** - Advanced compression features
- **LARGE_FILES_GUIDE.md** - Handling large files (10GB+)
- **MEMORY_SAFETY.md** - Memory safety guarantees

## 🔧 API Documentation

### Rust API
Located in `rust/docs/`:
- **[rust/docs/api/README_RUST_BINDINGS.md](rust/docs/api/README_RUST_BINDINGS.md)** - Complete Rust API reference
- **[rust/docs/guides/HOW_IT_WORKS.md](rust/docs/guides/HOW_IT_WORKS.md)** - Architecture and internals
- **[rust/docs/guides/C_INTEGRATION.md](rust/docs/guides/C_INTEGRATION.md)** - How C and Rust integrate
- **[rust/docs/guides/BUILD_GUIDE.md](rust/docs/guides/BUILD_GUIDE.md)** - Building the Rust library
- **[rust/docs/guides/QUICK_START.md](rust/docs/guides/QUICK_START.md)** - Rust quick start

### C API
- **[include/7z_ffi.h](include/7z_ffi.h)** - C header file with function declarations
- **[docs/api/](docs/api/)** - C API documentation

## 🔬 Development

### Contributing
Located in `docs/development/`:
- **CONTRIBUTING.md** - How to contribute
- **CODE_REVIEW_RECOMMENDATIONS.md** - Performance optimization recommendations
- **LZMA_SDK_OPTIMIZATION.md** - LZMA SDK optimization notes
- **TODO.md** - Planned features and improvements

### Implementation Details
Located in `docs/development/`:
- **IMPLEMENTATION_COMPLETE.md** - Implementation completion report
- **IMPLEMENTATION_GUIDE.md** - Step-by-step implementation guide
- **FINAL_SUMMARY.md** - Project summary

### Rust Development
Located in `rust/docs/development/`:
- **RUST_ENHANCEMENTS.md** - Rust library enhancements
- **RUST_ADDITIONS_SUMMARY.md** - Summary of Rust additions
- **OPTIMIZATION_RESULTS.md** - Performance optimization results
- **IMPLEMENTATION_STATUS.md** - Implementation status
- **COMPLETION_REPORT.md** - Completion report
- **CLEANUP_SUMMARY.md** - Code cleanup summary
- **INCOMPLETE_ANALYSIS.md** - Areas needing work

## 🧪 Testing

### Test Documentation
Located in `docs/testing/`:
- **TESTING_COMPLETE.md** - Complete testing guide
- **TEST_ANALYSIS_SUMMARY.md** - Test results and analysis
- **BENCHMARK_REPORT.md** - Performance benchmarks

### Rust Testing
Located in `rust/docs/testing/`:
- **TESTING.md** - Rust testing guide
- **TEST_COMPLETE_GUIDE.md** - Complete test guide
- **TEST_README.md** - Test suite overview
- **TEST_SUMMARY.md** - Test results summary

### Running Tests
```bash
# C library tests
cd /Users/terryreynolds/GitHub/sevenzip-ffi/build
ctest

# Rust tests
cd /Users/terryreynolds/GitHub/sevenzip-ffi/rust
cargo test

# Run all tests
./rust/scripts/run_all_tests.sh

# Benchmarks
cargo bench
./rust/scripts/run_all_benchmarks.sh
```

## 🔌 Integration

### Tauri Integration
Located in `tauri/`:
- **[tauri/TAURI_INTEGRATION_GUIDE.md](tauri/TAURI_INTEGRATION_GUIDE.md)** - Complete Tauri + SolidJS integration guide

## 📜 Scripts

Utility scripts located in `scripts/`:
- **demo.sh** - C library demonstration
- **demo_rust_crate.sh** - Rust library demonstration
- **scripts/setup_lzma.sh** - LZMA SDK setup
- **forensic_archiver_encrypted.sh** - Forensic archiving example
- **test_encryption_e2e.sh** - End-to-end encryption testing

Rust-specific scripts in `rust/scripts/`:
- **run_all_tests.sh** - Run complete test suite
- **run_all_benchmarks.sh** - Run all benchmarks
- **generate_test_report.sh** - Generate test reports
- **quick_bench.sh** - Quick benchmark
- **performance_test.sh** - Performance testing

## 📁 Project Structure

```
sevenzip-ffi/
├── README.md                    # Main project README
├── DOCUMENTATION.md             # This file
├── CMakeLists.txt              # C library build configuration
├── Cargo.toml                  # Workspace configuration
│
├── docs/                       # Root documentation
│   ├── guides/                 # User guides
│   ├── api/                    # API documentation
│   ├── development/            # Development docs
│   ├── testing/                # Test documentation
│   └── integration/            # Integration guides
│
├── src/                        # C source files
├── include/                    # C header files
├── lzma/                       # LZMA SDK
│
├── rust/                       # Rust bindings
│   ├── README.md              # Rust library README
│   ├── Cargo.toml             # Rust package config
│   ├── src/                   # Rust source code
│   ├── examples/              # Rust examples
│   ├── tests/                 # Rust tests
│   ├── benches/               # Rust benchmarks
│   ├── scripts/               # Rust utility scripts
│   └── docs/                  # Rust documentation
│       ├── guides/            # Rust guides
│       ├── api/               # Rust API docs
│       ├── development/       # Rust dev docs
│       └── testing/           # Rust test docs
│
├── tauri/                      # Tauri integration
│   └── TAURI_INTEGRATION_GUIDE.md
│
├── scripts/                    # Utility scripts
├── examples/                   # C examples
├── tests/                      # C tests
└── build/                      # Build artifacts
```

## 🎯 By Use Case

### I want to...

**...get started quickly**
1. [README.md](README.md)
2. [docs/guides/QUICKSTART.md](docs/guides/QUICKSTART.md)
3. [rust/docs/guides/QUICK_START.md](rust/docs/guides/QUICK_START.md)

**...use the Rust library**
1. [rust/README.md](rust/README.md)
2. [rust/docs/api/README_RUST_BINDINGS.md](rust/docs/api/README_RUST_BINDINGS.md)
3. [rust/examples/](rust/examples/)

**...understand how it works**
1. [rust/docs/guides/HOW_IT_WORKS.md](rust/docs/guides/HOW_IT_WORKS.md)
2. [rust/docs/guides/C_INTEGRATION.md](rust/docs/guides/C_INTEGRATION.md)
3. [docs/development/IMPLEMENTATION_COMPLETE.md](docs/development/IMPLEMENTATION_COMPLETE.md)

**...integrate with Tauri**
1. [tauri/TAURI_INTEGRATION_GUIDE.md](tauri/TAURI_INTEGRATION_GUIDE.md)

**...contribute to the project**
1. [docs/development/CONTRIBUTING.md](docs/development/CONTRIBUTING.md)
2. [docs/development/TODO.md](docs/development/TODO.md)
3. [docs/development/CODE_REVIEW_RECOMMENDATIONS.md](docs/development/CODE_REVIEW_RECOMMENDATIONS.md)

**...run tests**
1. [docs/testing/TESTING_COMPLETE.md](docs/testing/TESTING_COMPLETE.md)
2. [rust/docs/testing/TEST_COMPLETE_GUIDE.md](rust/docs/testing/TEST_COMPLETE_GUIDE.md)
3. `./rust/scripts/run_all_tests.sh`

**...optimize performance**
1. [docs/development/CODE_REVIEW_RECOMMENDATIONS.md](docs/development/CODE_REVIEW_RECOMMENDATIONS.md)
2. [docs/development/LZMA_SDK_OPTIMIZATION.md](docs/development/LZMA_SDK_OPTIMIZATION.md)
3. [rust/docs/development/OPTIMIZATION_RESULTS.md](rust/docs/development/OPTIMIZATION_RESULTS.md)

**...handle large files**
1. [docs/guides/LARGE_FILES_GUIDE.md](docs/guides/LARGE_FILES_GUIDE.md)
2. [docs/guides/ADVANCED_FEATURES.md](docs/guides/ADVANCED_FEATURES.md)

**...ensure memory safety**
1. [docs/guides/MEMORY_SAFETY.md](docs/guides/MEMORY_SAFETY.md)
2. [rust/docs/guides/HOW_IT_WORKS.md](rust/docs/guides/HOW_IT_WORKS.md)

## 📊 Status Reports

- **[docs/development/FINAL_SUMMARY.md](docs/development/FINAL_SUMMARY.md)** - Overall project status
- **[rust/docs/development/COMPLETION_REPORT.md](rust/docs/development/COMPLETION_REPORT.md)** - Rust implementation status
- **[docs/testing/TEST_ANALYSIS_SUMMARY.md](docs/testing/TEST_ANALYSIS_SUMMARY.md)** - Test results
- **[docs/testing/BENCHMARK_REPORT.md](docs/testing/BENCHMARK_REPORT.md)** - Performance benchmarks

## 🔗 External Resources

- [7-Zip Official Site](https://www.7-zip.org/)
- [LZMA SDK](https://www.7-zip.org/sdk.html)
- [Rust Documentation](https://doc.rust-lang.org/)
- [Tauri Documentation](https://tauri.app/)

## 📝 License

See [README.md](README.md) for license information.

---

**Last Updated:** January 31, 2026
**Version:** 1.2.0
