# Test suite for 7z FFI SDK
cmake_minimum_required(VERSION 3.15)

# Test executables - Original tests
add_executable(test_compress test_compress.c)
add_executable(test_extract test_extract.c)

# Test executables - New comprehensive tests
add_executable(test_filters test_filters.c)
add_executable(test_buffer_compression test_buffer_compression.c)
add_executable(test_selective_extraction test_selective_extraction.c)
add_executable(test_split_archive test_split_archive.c)
add_executable(test_repair test_repair.c)

# Link against our library
target_link_libraries(test_compress 7z_ffi)
target_link_libraries(test_extract 7z_ffi)
target_link_libraries(test_filters 7z_ffi)
target_link_libraries(test_buffer_compression 7z_ffi)
target_link_libraries(test_selective_extraction 7z_ffi)
target_link_libraries(test_split_archive 7z_ffi)
target_link_libraries(test_repair 7z_ffi)

# Add tests to CTest
enable_testing()

# Original tests
add_test(NAME compression_tests 
         COMMAND test_compress
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME extraction_tests 
         COMMAND test_extract
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# New comprehensive tests
add_test(NAME filter_tests
         COMMAND test_filters
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME buffer_compression_tests
         COMMAND test_buffer_compression
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME selective_extraction_tests
         COMMAND test_selective_extraction
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME split_archive_tests
         COMMAND test_split_archive
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME repair_tests
         COMMAND test_repair
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_compress test_extract test_filters test_buffer_compression 
            test_selective_extraction test_split_archive test_repair
    COMMENT "Running all unit tests..."
)
