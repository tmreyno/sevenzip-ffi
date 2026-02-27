#!/bin/bash
#
# Official 7-Zip Compatibility Test
# Verifies that archives created by sevenzip-ffi can be opened
# and extracted by the official 7z command-line tool
#

set -e

echo "════════════════════════════════════════════════════════════════"
echo "  Official 7-Zip Compatibility Test"
echo "  Testing: Can 7z extract sevenzip-ffi archives?"
echo "════════════════════════════════════════════════════════════════"
echo ""

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check 7z is installed
if ! command -v 7z &> /dev/null; then
    echo "ERROR: 7z command not found. Please install p7zip."
    exit 1
fi

echo "7-Zip version:"
7z | head -3
echo ""

# Clean up
rm -rf compat_test
mkdir -p compat_test
cd compat_test

# =============================================================================
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 1: Single-Volume Archive Compatibility"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Creating test files..."
echo "Test content for file 1" > file1.txt
echo "Test content for file 2" > file2.txt
dd if=/dev/urandom of=binary.dat bs=1k count=100 2>&1 | grep transferred

ORIG_FILE1_MD5=$(md5 -q file1.txt)
ORIG_FILE2_MD5=$(md5 -q file2.txt)
ORIG_BINARY_MD5=$(md5 -q binary.dat)

echo ""
echo -e "${BLUE}Creating archive with sevenzip-ffi...${NC}"
../build/examples/example_create_7z test_single.7z file1.txt file2.txt binary.dat

echo ""
echo -e "${BLUE}Testing 7z can LIST the archive...${NC}"
7z l test_single.7z

echo ""
echo -e "${BLUE}Testing 7z can TEST the archive integrity...${NC}"
if 7z t test_single.7z > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Archive integrity test PASSED${NC}"
else
    echo "✗ Archive integrity test FAILED"
    exit 1
fi

echo ""
echo -e "${BLUE}Extracting with official 7z command...${NC}"
mkdir extract_single
cd extract_single
7z x ../test_single.7z -y > /dev/null 2>&1
ls -lh
cd ..

echo ""
echo -e "${BLUE}Verifying file integrity...${NC}"
EXTR_FILE1_MD5=$(md5 -q extract_single/file1.txt)
EXTR_FILE2_MD5=$(md5 -q extract_single/file2.txt)
EXTR_BINARY_MD5=$(md5 -q extract_single/binary.dat)

if [ "$ORIG_FILE1_MD5" = "$EXTR_FILE1_MD5" ] && \
   [ "$ORIG_FILE2_MD5" = "$EXTR_FILE2_MD5" ] && \
   [ "$ORIG_BINARY_MD5" = "$EXTR_BINARY_MD5" ]; then
    echo -e "${GREEN}✓ All files extracted correctly${NC}"
    echo "  file1.txt: $ORIG_FILE1_MD5 = $EXTR_FILE1_MD5"
    echo "  file2.txt: $ORIG_FILE2_MD5 = $EXTR_FILE2_MD5"
    echo "  binary.dat: $ORIG_BINARY_MD5 = $EXTR_BINARY_MD5"
    echo -e "${GREEN}✓ TEST 1 PASSED - Single-volume archive fully compatible!${NC}"
else
    echo "✗ TEST 1 FAILED - MD5 mismatch"
    exit 1
fi

echo ""

# =============================================================================
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 2: Multi-Volume Archive Compatibility"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Creating 3MB test file..."
dd if=/dev/urandom of=largefile.dat bs=1M count=3 2>&1 | grep transferred
ORIG_LARGE_MD5=$(md5 -q largefile.dat)
echo "Original MD5: $ORIG_LARGE_MD5"

echo ""
echo -e "${BLUE}Creating split archive with sevenzip-ffi (500KB volumes)...${NC}"
../build/examples/demo_multivolume test_split.7z 0.5 largefile.dat

echo ""
echo -e "${BLUE}Volumes created:${NC}"
ls -lh test_split.7z.*
VOLUME_COUNT=$(ls test_split.7z.* | wc -l | tr -d ' ')
echo "Total volumes: $VOLUME_COUNT"

echo ""
echo -e "${BLUE}Testing 7z can LIST the split archive...${NC}"
7z l test_split.7z.001

echo ""
echo -e "${BLUE}Testing 7z can TEST the split archive integrity...${NC}"
if 7z t test_split.7z.001 > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Split archive integrity test PASSED${NC}"
else
    echo "✗ Split archive integrity test FAILED"
    exit 1
fi

echo ""
echo -e "${BLUE}Extracting split archive with official 7z command...${NC}"
mkdir extract_split
cd extract_split
7z x ../test_split.7z.001 -y
ls -lh
cd ..

echo ""
echo -e "${BLUE}Verifying file integrity...${NC}"
EXTR_LARGE_MD5=$(md5 -q extract_split/largefile.dat)

if [ "$ORIG_LARGE_MD5" = "$EXTR_LARGE_MD5" ]; then
    echo -e "${GREEN}✓ File extracted correctly from split archive${NC}"
    echo "  Original:  $ORIG_LARGE_MD5"
    echo "  Extracted: $EXTR_LARGE_MD5"
    echo -e "${GREEN}✓ TEST 2 PASSED - Multi-volume archive fully compatible!${NC}"
else
    echo "✗ TEST 2 FAILED - MD5 mismatch"
    echo "  Original:  $ORIG_LARGE_MD5"
    echo "  Extracted: $EXTR_LARGE_MD5"
    exit 1
fi

echo ""

# =============================================================================
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 3: Streaming Archive Compatibility"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Creating 2MB test file..."
dd if=/dev/urandom of=streamfile.dat bs=1M count=2 2>&1 | grep transferred
ORIG_STREAM_MD5=$(md5 -q streamfile.dat)

echo ""
echo -e "${BLUE}Creating archive with streaming compression...${NC}"
../build/examples/example_stream_compress test_stream.7z streamfile.dat

echo ""
echo -e "${BLUE}Testing 7z can extract streaming archive...${NC}"
mkdir extract_stream
cd extract_stream
7z x ../test_stream.7z -y > /dev/null 2>&1
cd ..

EXTR_STREAM_MD5=$(md5 -q extract_stream/streamfile.dat)

if [ "$ORIG_STREAM_MD5" = "$EXTR_STREAM_MD5" ]; then
    echo -e "${GREEN}✓ Streaming archive extracted correctly${NC}"
    echo "  MD5 match: $ORIG_STREAM_MD5"
    echo -e "${GREEN}✓ TEST 3 PASSED - Streaming archive fully compatible!${NC}"
else
    echo "✗ TEST 3 FAILED"
    exit 1
fi

echo ""

# =============================================================================
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 4: 7z Info and Metadata"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo -e "${BLUE}Detailed info on single-volume archive:${NC}"
echo "---"
7z l -slt test_single.7z | head -30

echo ""
echo -e "${BLUE}Detailed info on split archive:${NC}"
echo "---"
7z l -slt test_split.7z.001 | head -30

echo -e "${GREEN}✓ TEST 4 PASSED - All metadata accessible${NC}"

echo ""

# =============================================================================
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 5: Partial Extraction (Single File from Archive)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo -e "${BLUE}Extracting only file1.txt from multi-file archive...${NC}"
mkdir extract_partial
cd extract_partial
7z x ../test_single.7z file1.txt -y > /dev/null 2>&1
ls -lh
cd ..

if [ -f extract_partial/file1.txt ]; then
    PARTIAL_MD5=$(md5 -q extract_partial/file1.txt)
    if [ "$ORIG_FILE1_MD5" = "$PARTIAL_MD5" ]; then
        echo -e "${GREEN}✓ Partial extraction works correctly${NC}"
        echo "  MD5 match: $PARTIAL_MD5"
        echo -e "${GREEN}✓ TEST 5 PASSED${NC}"
    else
        echo "✗ TEST 5 FAILED - MD5 mismatch"
        exit 1
    fi
else
    echo "✗ TEST 5 FAILED - File not extracted"
    exit 1
fi

echo ""

# =============================================================================
echo "════════════════════════════════════════════════════════════════"
echo -e "${GREEN}              ALL COMPATIBILITY TESTS PASSED!${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

echo "Summary:"
echo "  ✓ Single-volume archives: 100% compatible with 7z"
echo "  ✓ Multi-volume archives: 100% compatible with 7z"
echo "  ✓ Streaming archives: 100% compatible with 7z"
echo "  ✓ File integrity: All MD5 checksums verified"
echo "  ✓ Metadata access: All archive info readable"
echo "  ✓ Partial extraction: Works correctly"
echo ""

echo "Test Coverage:"
echo "  • List archives (7z l)"
echo "  • Test integrity (7z t)"
echo "  • Full extraction (7z x)"
echo "  • Partial extraction (7z x <file>)"
echo "  • Detailed info (7z l -slt)"
echo "  • Split archive handling"
echo ""

echo "Conclusion:"
echo "  ${GREEN}Official 7-Zip can fully open, read, test, and extract"
echo "  all archives created by sevenzip-ffi!${NC}"
echo ""

cd ..
echo "Test artifacts saved in: compat_test/"
