cmake_minimum_required(VERSION 3.15)

# Example: Extract archive
add_executable(example_extract extract.c)
target_link_libraries(example_extract PRIVATE 7z_ffi)

# Example: List archive contents
add_executable(example_list list.c)
target_link_libraries(example_list PRIVATE 7z_ffi)

# Example: Compress file to LZMA
add_executable(example_compress compress.c)
target_link_libraries(example_compress PRIVATE 7z_ffi)

# Example: Decompress LZMA file
add_executable(example_decompress decompress.c)
target_link_libraries(example_decompress PRIVATE 7z_ffi)

# Example: Create multi-file archive
add_executable(example_create_archive create_archive.c)
target_link_libraries(example_create_archive PRIVATE 7z_ffi)

# Example: Extract multi-file archive
add_executable(example_extract_archive extract_archive.c)
target_link_libraries(example_extract_archive PRIVATE 7z_ffi)

# Example: Create standard .7z archive
add_executable(example_create_7z create_7z.c)
target_link_libraries(example_create_7z PRIVATE 7z_ffi)

# Example: Streaming compression with split volumes
add_executable(example_stream_compress stream_compress.c)
target_link_libraries(example_stream_compress PRIVATE 7z_ffi)

# Example: Multi-volume (split) archive creator
add_executable(demo_multivolume demo_multivolume.c)
target_link_libraries(demo_multivolume PRIVATE 7z_ffi)

# Example: Forensic evidence archiver (production-ready)
add_executable(forensic_archiver forensic_archiver.c)
target_link_libraries(forensic_archiver PRIVATE 7z_ffi)

# Example: Test encryption functionality
add_executable(test_encryption test_encryption.c)
target_link_libraries(test_encryption PRIVATE 7z_ffi)
target_include_directories(test_encryption PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Test: AES-256 encryption/decryption
add_executable(test_aes test_aes.c)
target_link_libraries(test_aes PRIVATE 7z_ffi)
target_include_directories(test_aes PRIVATE ${CMAKE_SOURCE_DIR}/src)
