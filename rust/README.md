# Rust Bindings Documentation

This directory contains comprehensive documentation for the Rust bindings of sevenzip-ffi.

## 📚 Quick Navigation

### 🚀 Getting Started
- **[docs/guides/QUICK_START.md](docs/guides/QUICK_START.md)** - Get up and running in 5 minutes
- **[docs/guides/BUILD_GUIDE.md](docs/guides/BUILD_GUIDE.md)** - Detailed build instructions
- **[docs/api/README_RUST_BINDINGS.md](docs/api/README_RUST_BINDINGS.md)** - Complete API reference

### 📖 Understanding the Library
- **[docs/guides/HOW_IT_WORKS.md](docs/guides/HOW_IT_WORKS.md)** - Architecture and design
- **[docs/guides/C_INTEGRATION.md](docs/guides/C_INTEGRATION.md)** - How Rust and C work together

### 🔧 Development
- **[docs/development/](docs/development/)** - Implementation details and enhancements
  - `RUST_ENHANCEMENTS.md` - Recent enhancements
  - `RUST_ADDITIONS_SUMMARY.md` - Summary of additions
  - `OPTIMIZATION_RESULTS.md` - Performance improvements
  - `COMPLETION_REPORT.md` - Feature completion status
  - `IMPLEMENTATION_STATUS.md` - Current implementation state

### 🧪 Testing
- **[docs/testing/](docs/testing/)** - Test documentation
  - `TEST_COMPLETE_GUIDE.md` - Complete testing guide
  - `TESTING.md` - Test overview
  - `TEST_SUMMARY.md` - Test results
- **[scripts/](scripts/)** - Test and benchmark scripts
  - `run_all_tests.sh` - Run complete test suite
  - `run_all_benchmarks.sh` - Run all benchmarks

## 📁 Directory Structure

```
rust/
├── docs/                       # All documentation
│   ├── guides/                # User guides
│   │   ├── QUICK_START.md
│   │   ├── BUILD_GUIDE.md
│   │   ├── HOW_IT_WORKS.md
│   │   └── C_INTEGRATION.md
│   ├── api/                   # API documentation
│   │   └── README_RUST_BINDINGS.md
│   ├── development/           # Development documentation
│   │   ├── RUST_ENHANCEMENTS.md
│   │   ├── RUST_ADDITIONS_SUMMARY.md
│   │   ├── OPTIMIZATION_RESULTS.md
│   │   ├── COMPLETION_REPORT.md
│   │   ├── IMPLEMENTATION_STATUS.md
│   │   ├── CLEANUP_SUMMARY.md
│   │   └── INCOMPLETE_ANALYSIS.md
│   └── testing/               # Test documentation
│       ├── TESTING.md
│       ├── TEST_COMPLETE_GUIDE.md
│       ├── TEST_README.md
│       └── TEST_SUMMARY.md
│
├── src/                       # Rust source code
│   ├── lib.rs                # Main library entry
│   ├── ffi.rs                # FFI bindings
│   ├── archive.rs            # Archive operations
│   ├── advanced.rs           # Advanced features
│   ├── encryption.rs         # C-based encryption
│   ├── encryption_native.rs  # Pure Rust encryption
│   └── error.rs              # Error types
│
├── examples/                  # Usage examples
│   ├── demo.rs               # Basic demo
│   ├── advanced_features.rs  # Advanced features demo
│   └── ...
│
├── tests/                     # Integration tests
│   └── integration_tests.rs
│
├── benches/                   # Performance benchmarks
│   └── compression_benchmarks.rs
│
├── scripts/                   # Utility scripts
│   ├── run_all_tests.sh
│   ├── run_all_benchmarks.sh
│   ├── quick_bench.sh
│   └── ...
│
├── Cargo.toml                # Package configuration
├── build.rs                  # Build script
└── README.md                 # This file
```

## 🎯 Quick Start

```bash
# Build the library
cargo build

# Run tests
cargo test

# Run benchmarks
cargo bench

# Run examples
cargo run --example demo

# Run all tests with script
./scripts/run_all_tests.sh
```

## 📖 Key Documentation Files

| File | Purpose | When to Read |
|------|---------|--------------|
| **docs/guides/QUICK_START.md** | Getting started | First time users |
| **docs/api/README_RUST_BINDINGS.md** | Complete API docs | Using the library |
| **docs/guides/HOW_IT_WORKS.md** | Architecture | Understanding internals |
| **docs/guides/C_INTEGRATION.md** | C/Rust integration | Contributing to FFI |
| **docs/development/RUST_ENHANCEMENTS.md** | Latest features | New features |
| **docs/testing/TEST_COMPLETE_GUIDE.md** | Testing guide | Running tests |

## 🔗 Related Documentation

- **[../DOCUMENTATION.md](../DOCUMENTATION.md)** - Complete project documentation index
- **[../README.md](../README.md)** - Main project README
- **[../CHANGELOG.md](../CHANGELOG.md)** - Release history

## 📝 Notes

All markdown files have been organized into the `docs/` directory with the following structure:
- **guides/** - User-facing guides and tutorials
- **api/** - API reference documentation
- **development/** - Internal development documentation
- **testing/** - Test-related documentation

For the complete documentation index, see [../DOCUMENTATION.md](../DOCUMENTATION.md).

---

**Last Updated:** March 6, 2026
