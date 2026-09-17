cmake_minimum_required(VERSION 3.20)
project(telemetry LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tm_warnings INTERFACE)
target_compile_options(tm_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Woverloaded-virtual -Werror
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wshadow-all>
)

add_library(tm_core
    src/core/core.cpp
)
target_include_directories(tm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(tm_core PUBLIC cxx_std_20)
target_link_libraries(tm_core PRIVATE tm_warnings)


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
    tests/core_test.cpp
)
target_link_libraries(tm_tests PRIVATE tm_core tm_warnings GTest::gtest_main)
gtest_discover_tests(tm_tests)
