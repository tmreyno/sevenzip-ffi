#!/bin/bash
#
# Multi-Volume Archive Demonstration
# Shows the working multi-volume creation feature
#

set -e

echo "════════════════════════════════════════════════════════════════"
echo "  Multi-Volume 7z Archive - Working Demo"
echo "════════════════════════════════════════════════════════════════"
echo ""

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Clean up
rm -rf demo_working
mkdir -p demo_working
cd demo_working

echo "Test 1: Single Large File with Multiple Volumes"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Creating 5MB test file..."
dd if=/dev/urandom of=testfile.dat bs=1M count=5 2>&1 | grep transferred
ls -lh testfile.dat
ORIG_MD5=$(md5 -q testfile.dat)
echo "Original MD5: $ORIG_MD5"

echo ""
echo -e "${BLUE}Creating archive with 1MB volumes...${NC}"
../build/examples/demo_multivolume archive1.7z 1 testfile.dat

echo ""
echo -e "${BLUE}Volumes created:${NC}"
ls -lh archive1.7z.*
VOLUME_COUNT=$(ls archive1.7z.* | wc -l | tr -d ' ')
echo -e "${GREEN}✓ Created $VOLUME_COUNT volumes${NC}"

echo ""
echo -e "${BLUE}Extracting with 7z...${NC}"
mkdir extract1
cd extract1
7z x ../archive1.7z.001 -y > /dev/null 2>&1
cd ..

EXTR_MD5=$(md5 -q extract1/testfile.dat)
echo "Extracted MD5: $EXTR_MD5"
if [ "$ORIG_MD5" = "$EXTR_MD5" ]; then
    echo -e "${GREEN}✓ TEST 1 PASSED - File integrity verified!${NC}"
else
    echo -e "✗ TEST 1 FAILED"
    exit 1
fi

echo ""
echo ""
echo "Test 2: Different Volume Size (500KB)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Creating 3MB test file..."
dd if=/dev/urandom of=testfile2.dat bs=1M count=3 2>&1 | grep transferred
ORIG_MD5=$(md5 -q testfile2.dat)

echo ""
echo -e "${BLUE}Creating archive with 500KB volumes...${NC}"
../build/examples/demo_multivolume archive2.7z 0.5 testfile2.dat

echo ""
echo -e "${BLUE}Volumes created:${NC}"
ls -lh archive2.7z.*
VOLUME_COUNT=$(ls archive2.7z.* | wc -l | tr -d ' ')
echo -e "${GREEN}✓ Created $VOLUME_COUNT volumes${NC}"

echo ""
echo -e "${BLUE}Extracting with 7z...${NC}"
mkdir extract2
cd extract2
7z x ../archive2.7z.001 -y > /dev/null 2>&1
cd ..

EXTR_MD5=$(md5 -q extract2/testfile2.dat)
if [ "$ORIG_MD5" = "$EXTR_MD5" ]; then
    echo -e "${GREEN}✓ TEST 2 PASSED - File integrity verified!${NC}"
else
    echo -e "✗ TEST 2 FAILED"
    exit 1
fi

echo ""
echo ""
echo "Test 3: Very Small Volumes (100KB - Stress Test)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Creating 1MB test file..."
dd if=/dev/urandom of=testfile3.dat bs=1M count=1 2>&1 | grep transferred
ORIG_MD5=$(md5 -q testfile3.dat)

echo ""
echo -e "${BLUE}Creating archive with 100KB volumes...${NC}"
../build/examples/demo_multivolume archive3.7z 0.1 testfile3.dat

echo ""
VOLUME_COUNT=$(ls archive3.7z.* | wc -l | tr -d ' ')
echo -e "${BLUE}Volumes created:${NC}"
ls archive3.7z.* | head -5
if [ $VOLUME_COUNT -gt 5 ]; then
    echo "... (total: $VOLUME_COUNT volumes)"
fi
echo -e "${GREEN}✓ Created $VOLUME_COUNT volumes${NC}"

echo ""
echo -e "${BLUE}Extracting with 7z...${NC}"
mkdir extract3
cd extract3
7z x ../archive3.7z.001 -y > /dev/null 2>&1
cd ..

EXTR_MD5=$(md5 -q extract3/testfile3.dat)
if [ "$ORIG_MD5" = "$EXTR_MD5" ]; then
    echo -e "${GREEN}✓ TEST 3 PASSED - ${VOLUME_COUNT} volumes extracted perfectly!${NC}"
else
    echo -e "✗ TEST 3 FAILED"
    exit 1
fi

echo ""
echo ""
echo "Test 4: Large File (10MB)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Creating 10MB test file..."
dd if=/dev/urandom of=largefile.dat bs=1M count=10 2>&1 | grep transferred
ls -lh largefile.dat
ORIG_MD5=$(md5 -q largefile.dat)

echo ""
echo -e "${BLUE}Creating archive with 2MB volumes...${NC}"
../build/examples/demo_multivolume archive4.7z 2 largefile.dat

echo ""
echo -e "${BLUE}Volumes created:${NC}"
ls -lh archive4.7z.*
VOLUME_COUNT=$(ls archive4.7z.* | wc -l | tr -d ' ')
echo -e "${GREEN}✓ Created $VOLUME_COUNT volumes${NC}"

echo ""
echo -e "${BLUE}Extracting with 7z...${NC}"
mkdir extract4
cd extract4
7z x ../archive4.7z.001 -y > /dev/null 2>&1
cd ..

EXTR_MD5=$(md5 -q extract4/largefile.dat)
if [ "$ORIG_MD5" = "$EXTR_MD5" ]; then
    echo -e "${GREEN}✓ TEST 4 PASSED - Large file extracted perfectly!${NC}"
else
    echo -e "✗ TEST 4 FAILED"
    exit 1
fi

echo ""
echo ""
echo "════════════════════════════════════════════════════════════════"
echo -e "${GREEN}                  ALL TESTS PASSED!${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Summary:"
echo "  ✓ Multi-volume creation works perfectly"
echo "  ✓ 100% compatible with 7-Zip extraction"
echo "  ✓ File integrity preserved across all volumes"
echo "  ✓ Supports various volume sizes (100KB to 2MB+)"
echo "  ✓ Handles large files (10MB+)"
echo "  ✓ Creates proper .7z.001, .7z.002, etc. naming"
echo ""
echo "Key Features Demonstrated:"
echo "  • Volume sizes from 100KB to 2MB"
echo "  • Up to 11 volumes created in stress test"
echo "  • All extractions verified with MD5 checksums"
echo "  • Compatible with standard 7-Zip tools"
echo ""

cd ..
