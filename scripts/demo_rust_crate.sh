#!/bin/bash
# Comprehensive demo of the seven-zip Rust crate

set -e

echo "================================================"
echo "  7z FFI SDK - Rust Crate Demonstration"
echo "================================================"
echo

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "Cargo.toml" ]; then
    echo "Error: Must run from project root (7z-ffi-sdk/)"
    exit 1
fi

echo -e "${BLUE}Building the Rust crate...${NC}"
cargo build --release
echo

echo -e "${GREEN}✓ Build successful!${NC}"
echo

echo "================================================"
echo "  Running Examples"
echo "================================================"
echo

# Test 1: Smoke test
echo -e "${BLUE}Test 1: Smoke Test${NC}"
echo "Running: cargo run --release --example smoke_test"
echo
cargo run --release --example smoke_test
echo

# Test 2: Create test files for demonstration
echo -e "${BLUE}Test 2: Creating test files...${NC}"
mkdir -p demo_test
echo "This is test file 1" > demo_test/file1.txt
echo "This is test file 2 with more content to compress" > demo_test/file2.txt
echo "AAAAAAAAAA" | head -c 10000 > demo_test/file3.txt  # Highly compressible
echo -e "${GREEN}✓ Created 3 test files in demo_test/${NC}"
echo

# Test 3: Compress single file
echo -e "${BLUE}Test 3: Single File Compression${NC}"
echo "Running: cargo run --release --example compress -- demo_test/file3.txt demo_test/file3.lzma2 maximum"
echo
cargo run --release --example compress -- demo_test/file3.txt demo_test/file3.lzma2 maximum
echo

# Test 4: Create multi-file archive
echo -e "${BLUE}Test 4: Multi-file Archive Creation${NC}"
echo "Running: cargo run --release --example create_archive -- demo_test/backup.7zff demo_test/file1.txt demo_test/file2.txt demo_test/file3.txt --level=normal"
echo
cargo run --release --example create_archive -- demo_test/backup.7zff demo_test/file1.txt demo_test/file2.txt demo_test/file3.txt --level=normal
echo

# Test 5: Library info
echo -e "${BLUE}Test 5: Library Information${NC}"
echo "Checking library size..."
ls -lh build/lib7z_ffi.a
echo
echo "Checking Rust library size..."
ls -lh target/release/libseven_zip.rlib
echo

echo "================================================"
echo -e "  ${GREEN}✅ All Demonstrations Complete!${NC}"
echo "================================================"
echo

echo "Summary:"
echo "  - Rust crate built successfully"
echo "  - Library initialization working"
echo "  - Compression utilities functional"
echo "  - Single-file compression working"
echo "  - Multi-file archive creation working"
echo

echo "Generated files in demo_test/:"
ls -lh demo_test/
echo

echo "To clean up: rm -rf demo_test/"
echo
echo "For more examples, see:"
echo "  cargo run --example extract -- <archive.7z> <output_dir>"
echo "  cargo run --example list -- <archive.7z>"
echo

echo -e "${GREEN}Rust crate is ready for use!${NC}"
