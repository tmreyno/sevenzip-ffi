# Test suite for 7z FFI SDK
cmake_minimum_required(VERSION 3.15)

# Test executables
add_executable(test_compress test_compress.c)
add_executable(test_extract test_extract.c)

# Link against our library
target_link_libraries(test_compress 7z_ffi)
target_link_libraries(test_extract 7z_ffi)

# Add tests to CTest
enable_testing()

add_test(NAME compression_tests 
         COMMAND test_compress
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME extraction_tests 
         COMMAND test_extract
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_compress test_extract
    COMMENT "Running all unit tests..."
)
