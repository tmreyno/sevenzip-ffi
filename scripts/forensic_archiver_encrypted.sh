#!/bin/bash
# Forensic Evidence Archiver with Encryption Wrapper
# 
# This script provides encrypted forensic archiving by combining
# the forensic_archiver with AES-256 encryption.

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORENSIC_ARCHIVER="$SCRIPT_DIR/build/examples/forensic_archiver"
CREATE_7Z="$SCRIPT_DIR/build/examples/example_create_7z"
EXTRACT_7Z="$SCRIPT_DIR/build/examples/example_extract"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print usage
usage() {
    echo "Forensic Evidence Archiver with AES-256 Encryption"
    echo "=================================================="
    echo ""
    echo "Usage: $0 <command> [options]"
    echo ""
    echo "Commands:"
    echo "  compress <archive> <files...>    Compress and encrypt files"
    echo "  extract <archive> <output_dir>   Decrypt and extract archive"
    echo ""
    echo "Compression Options:"
    echo "  --level <0-9>         Compression level (default: 5)"
    echo "  --split <size>        Split into volumes (e.g., 4g, 8g)"
    echo "  --threads <num>       Number of threads (default: 2)"
    echo "  --password <pass>     Password for encryption (prompts if not provided)"
    echo "  --resume              Enable resume capability"
    echo ""
    echo "Examples:"
    echo "  # Compress and encrypt evidence with 8GB splits:"
    echo "  $0 compress evidence.7z /path/to/evidence --split 8g --password"
    echo ""
    echo "  # Extract encrypted archive:"
    echo "  $0 extract evidence.7z.001.enc /output --password"
    echo ""
}

# Check if required tools exist
if [ ! -f "$CREATE_7Z" ]; then
    echo -e "${RED}Error: example_create_7z not found at $CREATE_7Z${NC}"
    echo "Please build the project first: cmake --build build"
    exit 1
fi

if [ ! -f "$EXTRACT_7Z" ]; then
    echo -e "${RED}Error: example_extract not found at $EXTRACT_7Z${NC}"
    echo "Please build the project first: cmake --build build"
    exit 1
fi

# Check for openssl
if ! command -v openssl &> /dev/null; then
    echo -e "${RED}Error: openssl not found${NC}"
    echo "Please install openssl: brew install openssl (macOS) or apt-get install openssl (Linux)"
    exit 1
fi

# Parse command
if [ $# -lt 1 ]; then
    usage
    exit 1
fi

COMMAND="$1"
shift

# Compress command
if [ "$COMMAND" = "compress" ]; then
    if [ $# -lt 2 ]; then
        echo -e "${RED}Error: compress requires <archive> and <files...>${NC}"
        usage
        exit 1
    fi
    
    ARCHIVE_PATH="$1"
    shift
    
    # Parse options
    ARGS=()
    PASSWORD=""
    PASSWORD_PROVIDED=0
    
    while [ $# -gt 0 ]; do
        case "$1" in
            --password)
                if [ $# -gt 1 ] && [[ ! "$2" =~ ^-- ]]; then
                    PASSWORD="$2"
                    PASSWORD_PROVIDED=1
                    shift 2
                else
                    # Prompt for password
                    read -s -p "Enter encryption password: " PASSWORD
                    echo ""
                    read -s -p "Confirm password: " PASSWORD_CONFIRM
                    echo ""
                    
                    if [ "$PASSWORD" != "$PASSWORD_CONFIRM" ]; then
                        echo -e "${RED}Error: Passwords do not match${NC}"
                        exit 1
                    fi
                    
                    if [ -z "$PASSWORD" ]; then
                        echo -e "${RED}Error: Password cannot be empty${NC}"
                        exit 1
                    fi
                    
                    PASSWORD_PROVIDED=1
                    shift
                fi
                ;;
            *)
                ARGS+=("$1")
                shift
                ;;
        esac
    done
    
    if [ $PASSWORD_PROVIDED -eq 0 ]; then
        echo -e "${YELLOW}Warning: No password specified. Archive will NOT be encrypted.${NC}"
        echo "Use --password flag to enable encryption."
        echo ""
    fi
    
    # Run example_create_7z to create standard 7z archive
    echo -e "${GREEN}Step 1/2: Creating 7z archive...${NC}"
    "$CREATE_7Z" "$ARCHIVE_PATH" "${ARGS[@]}"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Archive creation failed${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Archive created successfully!${NC}"
    
    # Encrypt volumes if password provided
    if [ $PASSWORD_PROVIDED -eq 1 ]; then
        echo ""
        echo -e "${GREEN}Step 2/2: Encrypting volumes...${NC}"
        
        # Find all volumes
        VOLUMES=("$ARCHIVE_PATH")
        if [ -f "${ARCHIVE_PATH}.001" ]; then
            VOLUMES=(${ARCHIVE_PATH}.*)
        fi
        
        for VOLUME in "${VOLUMES[@]}"; do
            if [ ! -f "$VOLUME" ]; then
                continue
            fi
            
            echo "  Encrypting: $VOLUME"
            
            # Encrypt with AES-256-CBC, PBKDF2, 262144 iterations (7-Zip standard)
            openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -salt \
                -in "$VOLUME" -out "${VOLUME}.enc" \
                -pass pass:"$PASSWORD" 2>&1
            
            if [ $? -eq 0 ]; then
                # Remove unencrypted file
                rm "$VOLUME"
                echo -e "    ${GREEN}✓${NC} Encrypted: ${VOLUME}.enc"
            else
                echo -e "    ${RED}✗${NC} Failed to encrypt: $VOLUME"
                exit 1
            fi
        done
        
        echo ""
        echo -e "${GREEN}✓ Compression and encryption completed successfully!${NC}"
        echo "Encrypted archives:"
        ls -lh "${ARCHIVE_PATH}"*.enc 2>/dev/null || ls -lh "${ARCHIVE_PATH}.enc"
    else
        echo ""
        echo -e "${GREEN}✓ Compression completed successfully!${NC}"
    fi

# Extract command
elif [ "$COMMAND" = "extract" ]; then
    if [ $# -lt 2 ]; then
        echo -e "${RED}Error: extract requires <archive> and <output_dir>${NC}"
        usage
        exit 1
    fi
    
    ARCHIVE_PATH="$1"
    OUTPUT_DIR="$2"
    shift 2
    
    # Check if archive is encrypted
    IS_ENCRYPTED=0
    if [[ "$ARCHIVE_PATH" == *.enc ]]; then
        IS_ENCRYPTED=1
    elif [ ! -f "$ARCHIVE_PATH" ] && [ -f "${ARCHIVE_PATH}.enc" ]; then
        ARCHIVE_PATH="${ARCHIVE_PATH}.enc"
        IS_ENCRYPTED=1
    fi
    
    # Parse options
    PASSWORD=""
    PASSWORD_PROVIDED=0
    
    while [ $# -gt 0 ]; do
        case "$1" in
            --password)
                if [ $# -gt 1 ] && [[ ! "$2" =~ ^-- ]]; then
                    PASSWORD="$2"
                    PASSWORD_PROVIDED=1
                    shift 2
                else
                    read -s -p "Enter decryption password: " PASSWORD
                    echo ""
                    
                    if [ -z "$PASSWORD" ]; then
                        echo -e "${RED}Error: Password cannot be empty${NC}"
                        exit 1
                    fi
                    
                    PASSWORD_PROVIDED=1
                    shift
                fi
                ;;
            *)
                shift
                ;;
        esac
    done
    
    if [ $IS_ENCRYPTED -eq 1 ]; then
        if [ $PASSWORD_PROVIDED -eq 0 ]; then
            read -s -p "Archive is encrypted. Enter password: " PASSWORD
            echo ""
            
            if [ -z "$PASSWORD" ]; then
                echo -e "${RED}Error: Password required for encrypted archive${NC}"
                exit 1
            fi
            PASSWORD_PROVIDED=1
        fi
        
        echo -e "${GREEN}Step 1/2: Decrypting volumes...${NC}"
        
        # Find all encrypted volumes
        BASE_PATH="${ARCHIVE_PATH%.enc}"
        VOLUMES=("$ARCHIVE_PATH")
        if [ -f "${BASE_PATH}.001.enc" ]; then
            VOLUMES=(${BASE_PATH}.*.enc)
        fi
        
        for ENC_VOLUME in "${VOLUMES[@]}"; do
            if [ ! -f "$ENC_VOLUME" ]; then
                continue
            fi
            
            DEC_VOLUME="${ENC_VOLUME%.enc}"
            echo "  Decrypting: $ENC_VOLUME"
            
            # Decrypt
            openssl enc -aes-256-cbc -pbkdf2 -iter 262144 -d -salt \
                -in "$ENC_VOLUME" -out "$DEC_VOLUME" \
                -pass pass:"$PASSWORD" 2>&1
            
            if [ $? -eq 0 ]; then
                echo -e "    ${GREEN}✓${NC} Decrypted: $DEC_VOLUME"
            else
                echo -e "    ${RED}✗${NC} Failed to decrypt: $ENC_VOLUME"
                echo "Wrong password or corrupted file."
                
                # Clean up partial decryption
                rm -f "${BASE_PATH}".* 2>/dev/null
                exit 1
            fi
        done
        
        echo ""
        echo -e "${GREEN}Step 2/2: Extracting archive...${NC}"
        
        # Extract using example_extract
        DECRYPTED_ARCHIVE="${BASE_PATH}"
        if [ -f "${BASE_PATH}.001" ]; then
            DECRYPTED_ARCHIVE="${BASE_PATH}.001"
        fi
        
        "$EXTRACT_7Z" "$DECRYPTED_ARCHIVE" "$OUTPUT_DIR"
        EXTRACT_RESULT=$?
        
        # Clean up decrypted files
        echo ""
        echo "Cleaning up decrypted temporary files..."
        rm -f "${BASE_PATH}".* 2>/dev/null
        
        if [ $EXTRACT_RESULT -eq 0 ]; then
            echo ""
            echo -e "${GREEN}✓ Decryption and extraction completed successfully!${NC}"
        else
            echo -e "${RED}Error: Extraction failed${NC}"
            exit 1
        fi
    else
        # Not encrypted, extract directly
        echo -e "${GREEN}Extracting archive...${NC}"
        "$EXTRACT_7Z" "$ARCHIVE_PATH" "$OUTPUT_DIR"
        
        if [ $? -eq 0 ]; then
            echo ""
            echo -e "${GREEN}✓ Extraction completed successfully!${NC}"
        else
            echo -e "${RED}Error: Extraction failed${NC}"
            exit 1
        fi
    fi

else
    echo -e "${RED}Error: Unknown command '$COMMAND'${NC}"
    usage
    exit 1
fi
