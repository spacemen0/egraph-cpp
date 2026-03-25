cmake_minimum_required(VERSION 3.15)
project(egraph-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)

if (TARGET gtest)
    set_property(TARGET gtest PROPERTY CXX_CLANG_TIDY "")
endif()

if (TARGET gtest_main)
    set_property(TARGET gtest_main PROPERTY CXX_CLANG_TIDY "")
endif()
add_compile_options(--coverage -g)
add_link_options(--coverage)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building with AddressSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
else()
    message(STATUS "Release mode: Building with optimizations")
    add_compile_options(-O3)
endif()

add_subdirectory(src)
add_subdirectory(tests)