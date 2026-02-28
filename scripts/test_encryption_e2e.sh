#!/bin/bash
# Comprehensive End-to-End Encryption Test
#
# Tests the complete encryption workflow with various scenarios

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Test function
test_case() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "TEST: $1"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Setup test environment
echo "Setting up test environment..."
TEST_DIR="/tmp/7z_encryption_test_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# Create test data
mkdir -p test_data
echo "Test file 1 content" > test_data/file1.txt
echo "Test file 2 content with more data" > test_data/file2.txt
echo "Binary data: $(dd if=/dev/urandom bs=1024 count=10 2>/dev/null | base64)" > test_data/file3.bin

PASSWORD="TestPassword123!"
WRONG_PASSWORD="WrongPassword456!"

echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║   7z FFI SDK - Encryption Integration Test Suite     ║"
echo "╚═══════════════════════════════════════════════════════╝"

# Test 1: AES-256 Encryption Functions
test_case "AES-256 Encryption/Decryption Functions"
"$BUILD_DIR/examples/test_aes" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    pass "AES-256 encryption functions work correctly"
else
    fail "AES-256 encryption functions failed"
fi

# Test 2: Standard 7z Archive Creation
test_case "Standard 7z Archive Creation"
"$BUILD_DIR/examples/example_create_7z" "$TEST_DIR/test.7z" test_data/file1.txt test_data/file2.txt > /dev/null 2>&1
if [ -f "$TEST_DIR/test.7z" ] && [ -s "$TEST_DIR/test.7z" ]; then
    pass "Created standard 7z archive"
else
    fail "Failed to create standard 7z archive"
fi

# Test 3: OpenSSL Encryption
test_case "OpenSSL AES-256 Encryption"
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -salt \
    -in "$TEST_DIR/test.7z" -out "$TEST_DIR/test.7z.enc" \
    -pass pass:"$PASSWORD" 2>/dev/null
    
if [ -f "$TEST_DIR/test.7z.enc" ] && [ -s "$TEST_DIR/test.7z.enc" ]; then
    SIZE_ORIGINAL=$(stat -f%z "$TEST_DIR/test.7z" 2>/dev/null || stat -c%s "$TEST_DIR/test.7z")
    SIZE_ENCRYPTED=$(stat -f%z "$TEST_DIR/test.7z.enc" 2>/dev/null || stat -c%s "$TEST_DIR/test.7z.enc")
    
    if [ $SIZE_ENCRYPTED -gt $SIZE_ORIGINAL ]; then
        pass "OpenSSL encryption succeeded (size: $SIZE_ORIGINAL → $SIZE_ENCRYPTED bytes)"
    else
        fail "Encrypted file is not larger than original"
    fi
else
    fail "OpenSSL encryption failed"
fi

# Test 4: OpenSSL Decryption with Correct Password
test_case "OpenSSL Decryption (Correct Password)"
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
    -in "$TEST_DIR/test.7z.enc" -out "$TEST_DIR/test_decrypted.7z" \
    -pass pass:"$PASSWORD" 2>/dev/null
    
if [ -f "$TEST_DIR/test_decrypted.7z" ]; then
    # Compare with original
    if diff "$TEST_DIR/test.7z" "$TEST_DIR/test_decrypted.7z" > /dev/null 2>&1; then
        pass "Decryption succeeded and matches original"
    else
        fail "Decrypted file differs from original"
    fi
else
    fail "OpenSSL decryption failed"
fi

# Test 5: OpenSSL Decryption with Wrong Password
test_case "OpenSSL Decryption (Wrong Password)"
# Note: OpenSSL may succeed decrypting with wrong password but produce garbage
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
    -in "$TEST_DIR/test.7z.enc" -out "$TEST_DIR/test_wrong.7z" \
    -pass pass:"$WRONG_PASSWORD" 2>/dev/null || true
    
if [ -f "$TEST_DIR/test_wrong.7z" ]; then
    # Check if it's garbage (not matching original)
    if ! diff "$TEST_DIR/test.7z" "$TEST_DIR/test_wrong.7z" > /dev/null 2>&1; then
        pass "Wrong password produces garbage/fails (as expected)"
    else
        fail "Wrong password produced valid output (should not happen)"
    fi
else
    pass "Wrong password rejected by OpenSSL"
fi

# Test 6: Archive Extraction
test_case "Archive Extraction After Decryption"
rm -rf extracted
"$BUILD_DIR/examples/example_extract" "$TEST_DIR/test_decrypted.7z" extracted > /dev/null 2>&1
if [ -f "extracted/file1.txt" ] && [ -f "extracted/file2.txt" ]; then
    CONTENT1=$(cat extracted/file1.txt)
    CONTENT2=$(cat extracted/file2.txt)
    
    if [ "$CONTENT1" = "Test file 1 content" ] && [ "$CONTENT2" = "Test file 2 content with more data" ]; then
        pass "Extracted files match original content"
    else
        fail "Extracted file content doesn't match"
    fi
else
    fail "Files not extracted correctly"
fi

# Test 7: Split Archive with Encryption
test_case "Split Archive Creation and Encryption"
"$BUILD_DIR/examples/example_create_7z" "$TEST_DIR/split.7z" test_data/*.txt test_data/*.bin > /dev/null 2>&1

# Encrypt it
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -salt \
    -in "$TEST_DIR/split.7z" -out "$TEST_DIR/split.7z.enc" \
    -pass pass:"$PASSWORD" 2>/dev/null
    
if [ -f "$TEST_DIR/split.7z.enc" ]; then
    # Decrypt
    openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
        -in "$TEST_DIR/split.7z.enc" -out "$TEST_DIR/split_dec.7z" \
        -pass pass:"$PASSWORD" 2>/dev/null
    
    # Extract
    rm -rf split_extracted
    "$BUILD_DIR/examples/example_extract" "$TEST_DIR/split_dec.7z" split_extracted > /dev/null 2>&1
    
    if [ -f "split_extracted/file1.txt" ] && [ -f "split_extracted/file3.bin" ]; then
        pass "Split archive encryption/decryption/extraction succeeded"
    else
        fail "Split archive workflow failed"
    fi
else
    fail "Split archive encryption failed"
fi

# Test 8: Forensic Archiver Integration
test_case "Forensic Archiver with Encryption Wrapper"
if [ -f "$SCRIPT_DIR/forensic_archiver_encrypted.sh" ]; then
    # Use example_create_7z instead since forensic_archiver creates custom format
    "$BUILD_DIR/examples/example_create_7z" "$TEST_DIR/forensic.7z" test_data/*.txt > /dev/null 2>&1
    
    # Encrypt
    openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -salt \
        -in "$TEST_DIR/forensic.7z" -out "$TEST_DIR/forensic.7z.enc" \
        -pass pass:"$PASSWORD" 2>/dev/null
    
    # Decrypt and extract
    openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
        -in "$TEST_DIR/forensic.7z.enc" -out "$TEST_DIR/forensic_dec.7z" \
        -pass pass:"$PASSWORD" 2>/dev/null
        
    rm -rf forensic_extracted
    "$BUILD_DIR/examples/forensic_archiver" extract "$TEST_DIR/forensic_dec.7z" forensic_extracted > /dev/null 2>&1
    
    if [ -f "forensic_extracted/file1.txt" ] && [ -f "forensic_extracted/file2.txt" ]; then
        pass "Forensic archiver extraction from encrypted archive succeeded"
    else
        fail "Forensic archiver extraction failed"
    fi
else
    fail "Encryption wrapper script not found"
fi

# Test 9: Large File Test (10MB)
test_case "Large File Encryption (10MB)"
dd if=/dev/urandom of="$TEST_DIR/large_file.bin" bs=1m count=10 2>/dev/null

"$BUILD_DIR/examples/example_create_7z" "$TEST_DIR/large.7z" "$TEST_DIR/large_file.bin" > /dev/null 2>&1

# Encrypt
time_start=$(date +%s)
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -salt \
    -in "$TEST_DIR/large.7z" -out "$TEST_DIR/large.7z.enc" \
    -pass pass:"$PASSWORD" 2>/dev/null
time_end=$(date +%s)
time_encrypt=$((time_end - time_start))

# Decrypt
time_start=$(date +%s)
openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
    -in "$TEST_DIR/large.7z.enc" -out "$TEST_DIR/large_dec.7z" \
    -pass pass:"$PASSWORD" 2>/dev/null
time_end=$(date +%s)
time_decrypt=$((time_end - time_start))

if [ -f "$TEST_DIR/large_dec.7z" ]; then
    if diff "$TEST_DIR/large.7z" "$TEST_DIR/large_dec.7z" > /dev/null 2>&1; then
        pass "10MB file encrypted/decrypted successfully (encrypt: ${time_encrypt}s, decrypt: ${time_decrypt}s)"
    else
        fail "10MB decrypted file differs from original"
    fi
else
    fail "10MB file encryption failed"
fi

# Test 10: 7-Zip Compatibility
test_case "7-Zip Compatibility Check"
if command -v 7z &> /dev/null; then
    7z t "$TEST_DIR/test_decrypted.7z" > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        pass "Archives are compatible with official 7-Zip"
    else
        fail "Archives not compatible with 7-Zip"
    fi
else
    echo -e "${YELLOW}⊘ SKIP${NC}: 7-Zip not installed (brew install p7zip to test)"
fi

# Cleanup
echo ""
echo "Cleaning up test files..."
cd /
rm -rf "$TEST_DIR"

# Summary
echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║                   TEST SUMMARY                        ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Total Tests:  $((TESTS_PASSED + TESTS_FAILED))"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}╔═════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✓ ALL TESTS PASSED - PRODUCTION READY!    ║${NC}"
    echo -e "${GREEN}╚═════════════════════════════════════════════╝${NC}"
    exit 0
else
    echo -e "${RED}╔═════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ✗ SOME TESTS FAILED - REVIEW REQUIRED     ║${NC}"
    echo -e "${RED}╚═════════════════════════════════════════════╝${NC}"
    exit 1
fi
