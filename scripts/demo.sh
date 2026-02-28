#!/bin/bash

# Demo script for 7z FFI SDK
# This script demonstrates the basic functionality of the library

set -e

echo "======================================"
echo "7z FFI SDK - Demo"
echo "======================================"
echo ""

# Check if build exists
if [ ! -f "build/lib7z_ffi.dylib" ]; then
    echo "Error: Library not built. Please run:"
    echo "  mkdir build && cd build && cmake .. && cmake --build ."
    exit 1
fi

# Check if test archive exists
if [ ! -f "test_archive.7z" ]; then
    echo "Creating test archive..."
    7z a test_archive.7z test_data/ > /dev/null
fi

echo "==================================="
echo "1. Testing Archive Compression"
echo "==================================="
echo "Compressing a single file to LZMA format..."
rm -f demo_compressed.lzma
./build/examples/example_compress demo_compressed.lzma test_data/readme.md
echo ""

echo ""
echo "==================================="
echo "2. Testing Archive Listing"
echo "==================================="
./build/examples/example_list test_archive.7z
echo ""

echo ""
echo "==================================="
echo "3. Testing Archive Extraction"
echo "==================================="
rm -rf demo_extracted
./build/examples/example_extract test_archive.7z demo_extracted
echo ""

echo ""
echo "==================================="
echo "4. Verifying Extracted Files"
echo "==================================="
echo "Files extracted to: demo_extracted/"
find demo_extracted -type f -exec echo "  ✓ {}" \;
echo ""

echo ""
echo "==================================="
echo "5. File Contents Verification"
echo "==================================="
echo "First file content:"
head -3 demo_extracted/test_data/test1.txt
echo ""

echo ""
echo "==================================="
echo "6. Decompressing LZMA File"
echo "==================================="
echo "Using library to decompress..."
./build/examples/example_decompress demo_compressed.lzma demo_decompressed_lib.txt
echo ""
echo "Decompressed content:"
cat demo_decompressed_lib.txt
rm -f demo_decompressed_lib.txt
echo ""

echo ""
echo "======================================"
echo "Demo Complete!"
echo "======================================"
echo ""
echo "Library location: build/lib7z_ffi.dylib"
echo "Header file: include/7z_ffi.h"
echo "Examples: build/examples/"
echo ""
echo "Features demonstrated:"
echo "  ✓ Single file compression (LZMA format)"
echo "  ✓ Single file decompression (LZMA format)"
echo "  ✓ Archive listing (7z format)"
echo "  ✓ Archive extraction (7z format)"
echo "  ✓ Progress callbacks"
echo "  ✓ File verification"
echo ""
echo "Next steps:"
echo "  - Integrate with Tauri (see tauri/tauri-integration.md)"
echo "  - Use in your C/C++ project"
echo "  - Build for other platforms"
echo "  - Multiple file compression (coming in v1.1.0)"
echo ""
