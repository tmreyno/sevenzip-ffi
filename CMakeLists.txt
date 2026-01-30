cmake_minimum_required(VERSION 3.15)
project(7z_ffi_sdk VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example applications" ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lzma/C
)

# LZMA SDK source files
set(LZMA_SOURCES
    lzma/C/7zAlloc.c
    lzma/C/7zArcIn.c
    lzma/C/7zBuf.c
    lzma/C/7zBuf2.c
    lzma/C/7zCrc.c
    lzma/C/7zCrcOpt.c
    lzma/C/7zDec.c
    lzma/C/7zFile.c
    lzma/C/7zStream.c
    lzma/C/Aes.c
    lzma/C/AesOpt.c
    lzma/C/Alloc.c
    lzma/C/Bcj2.c
    lzma/C/Bra.c
    lzma/C/Bra86.c
    lzma/C/BraIA64.c
    lzma/C/CpuArch.c
    lzma/C/Delta.c
    lzma/C/LzFind.c
    lzma/C/LzFindMt.c
    lzma/C/LzFindOpt.c
    lzma/C/Lzma2Dec.c
    lzma/C/Lzma2DecMt.c
    lzma/C/Lzma2Enc.c
    lzma/C/Lzma86Dec.c
    lzma/C/Lzma86Enc.c
    lzma/C/LzmaDec.c
    lzma/C/LzmaEnc.c
    lzma/C/LzmaLib.c
    lzma/C/MtCoder.c
    lzma/C/MtDec.c
    lzma/C/Ppmd7.c
    lzma/C/Ppmd7Dec.c
    lzma/C/Ppmd7Enc.c
    lzma/C/Sha256.c
    lzma/C/Sha256Opt.c
    lzma/C/Threads.c
    lzma/C/Xz.c
    lzma/C/XzCrc64.c
    lzma/C/XzCrc64Opt.c
    lzma/C/XzDec.c
    lzma/C/XzEnc.c
    lzma/C/XzIn.c
)

# FFI wrapper source files (organized by function)
set(FFI_SOURCES
    # Core API
    src/ffi_interface.c
    src/error_reporting.c
    
    # Archive operations
    src/archive_create.c
    src/archive_create_custom.c
    src/archive_create_multivolume.c
    src/archive_extract.c
    src/archive_extract_custom.c
    src/archive_extract_split.c
    src/archive_list.c
    src/archive_test.c
    src/archive_stream_api.c
    
    # Compression
    src/lzma_compress.c
    src/lzma_decompress.c
    
    # Security
    src/encryption_aes.c
)

# Create the library
add_library(7z_ffi ${LZMA_SOURCES} ${FFI_SOURCES})

# Set library properties
set_target_properties(7z_ffi PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/7z_ffi.h
)

# Platform-specific settings
if(WIN32)
    # Enable multi-threading for Windows
    target_compile_definitions(7z_ffi PRIVATE _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(7z_ffi PRIVATE DLL_EXPORT)
    endif()
elseif(UNIX)
    # Enable multi-threading and optimizations for Unix/macOS
    target_compile_definitions(7z_ffi PRIVATE _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
    target_link_libraries(7z_ffi PRIVATE pthread)
    
    # Enable ARM optimizations on Apple Silicon / ARM64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
        message(STATUS "Enabling ARM64 optimizations (CRC32, NEON)")
        target_compile_options(7z_ffi PRIVATE -march=armv8-a+crc+crypto)
        target_compile_definitions(7z_ffi PRIVATE 
            MY_CPU_ARM64
            __ARM_FEATURE_CRC32=1
        )
    endif()
    
    # Aggressive optimizations for release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(7z_ffi PRIVATE 
            -O3
            -ffast-math
            -funroll-loops
            -fomit-frame-pointer
        )
    endif()
endif()

# Installation
install(TARGETS 7z_ffi
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

# Build examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
