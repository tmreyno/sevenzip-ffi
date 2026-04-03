#!/bin/bash

# Script to download and setup LZMA SDK for 7z FFI SDK
# Usage: ./setup_lzma.sh

set -e

LZMA_VERSION="2301"
LZMA_URL="https://www.7-zip.org/a/lzma${LZMA_VERSION}.7z"
LZMA_FILE="lzma${LZMA_VERSION}.7z"

echo "================================"
echo "LZMA SDK Setup Script"
echo "================================"
echo ""

if [ -d "lzma/C" ] && [ -f "lzma/C/7z.h" ]; then
    echo "Vendored LZMA SDK already present in lzma/C; skipping download."
    exit 0
fi

# Check if 7z is installed
if ! command -v 7z &> /dev/null; then
    echo "Error: 7z command not found!"
    echo "Please install p7zip first:"
    echo "  macOS: brew install p7zip"
    echo "  Ubuntu/Debian: sudo apt-get install p7zip-full"
    echo "  Fedora/RHEL: sudo dnf install p7zip"
    exit 1
fi

echo "Downloading LZMA SDK version ${LZMA_VERSION}..."
curl -L -o "$LZMA_FILE" "$LZMA_URL"

if [ ! -f "$LZMA_FILE" ]; then
    echo "Error: Failed to download LZMA SDK"
    exit 1
fi

echo "Extracting LZMA SDK..."
7z x "$LZMA_FILE" -o"lzma_temp" -y

if [ ! -d "lzma_temp/C" ]; then
    echo "Error: Extracted LZMA SDK does not contain expected 'C' directory"
    exit 1
fi

echo "Copying C files to project..."
mkdir -p lzma
cp -r lzma_temp/C lzma/

echo "Cleaning up temporary files..."
rm -rf lzma_temp "$LZMA_FILE"

echo ""
echo "================================"
echo "LZMA SDK setup complete!"
echo "================================"
echo ""
echo "You can now build the project:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
