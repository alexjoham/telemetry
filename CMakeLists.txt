cmake_minimum_required(VERSION 3.20)
project(telemetry LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tm_warnings INTERFACE)
target_compile_options(tm_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Woverloaded-virtual -Werror
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wshadow-all>
)

add_library(tm_core INTERFACE)
target_include_directories(tm_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(tm_core INTERFACE cxx_std_20)


# Headers as their own TUs, linked by nothing: misc-include-cleaner only inspects a TU's main file.
# CONFIGURE_DEPENDS, or a header added after the last configure is silently exempt.
file(GLOB_RECURSE TM_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)
set_source_files_properties(${TM_HEADERS} PROPERTIES LANGUAGE CXX)
add_library(tm_header_tus OBJECT ${TM_HEADERS})
target_include_directories(tm_header_tus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(tm_header_tus PRIVATE cxx_std_20)
target_link_libraries(tm_header_tus PRIVATE tm_warnings)


include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.18.0
)
set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
FetchContent_MakeAvailable(googletest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
include(GoogleTest)
add_executable(tm_tests
    tests/crc_test.cpp
    tests/framer_test.cpp
    tests/recovery_test.cpp
    tests/result_test.cpp
)
target_link_libraries(tm_tests PRIVATE tm_core tm_warnings GTest::gtest_main)
gtest_discover_tests(tm_tests)
