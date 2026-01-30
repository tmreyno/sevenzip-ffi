# Setup Instructions

## Prerequisites

Before building the 7z FFI SDK, you need to download the LZMA SDK from 7-Zip.

## Download LZMA SDK

1. Visit the 7-Zip SDK download page:
   - Official: https://www.7-zip.org/sdk.html
   - Direct download: https://www.7-zip.org/a/lzma2301.7z

2. Download the latest LZMA SDK (e.g., `lzma2301.7z`)

3. Extract the archive

4. Copy the `C` directory from the extracted SDK to this project:
   ```bash
   cp -r /path/to/extracted/lzma/C ./lzma/C
   ```

   Your directory structure should look like:
   ```
   sevenzip-ffi/
   ├── lzma/
   │   └── C/
   │       ├── 7z.h
   │       ├── 7zAlloc.c
   │       ├── 7zCrc.c
   │       └── ... (other LZMA SDK files)
   ├── src/
   ├── include/
   └── CMakeLists.txt
   ```

## Alternative: Script to Download

You can use this script to automatically download and set up the LZMA SDK:

```bash
#!/bin/bash

# Download LZMA SDK
LZMA_VERSION="2301"
LZMA_URL="https://www.7-zip.org/a/lzma${LZMA_VERSION}.7z"
LZMA_FILE="lzma${LZMA_VERSION}.7z"

echo "Downloading LZMA SDK..."
curl -L -o "$LZMA_FILE" "$LZMA_URL"

echo "Extracting LZMA SDK..."
7z x "$LZMA_FILE" -o"lzma_temp"

echo "Copying C files..."
mkdir -p lzma
cp -r lzma_temp/C lzma/

echo "Cleaning up..."
rm -rf lzma_temp "$LZMA_FILE"

echo "LZMA SDK setup complete!"
```

Save this as `setup_lzma.sh`, make it executable with `chmod +x setup_lzma.sh`, and run it.

## Build

Once you have the LZMA SDK in place:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Verify

Check that the library was built successfully:

- Linux: `build/lib7z_ffi.so`
- macOS: `build/lib7z_ffi.dylib`
- Windows: `build/Release/7z_ffi.dll`

## Test

Run the example programs:

```bash
# List archive contents
./build/examples/example_list path/to/archive.7z

# Extract archive
./build/examples/example_extract path/to/archive.7z output_directory
```
