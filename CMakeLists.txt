cmake_minimum_required(VERSION 3.15)
project(egraph-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)

find_program(CLANG_TIDY "clang-tidy")

if (CLANG_TIDY)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
endif()

if (TARGET gtest)
    set_property(TARGET gtest PROPERTY CXX_CLANG_TIDY "")
endif()

if (TARGET gtest_main)
    set_property(TARGET gtest_main PROPERTY CXX_CLANG_TIDY "")
endif()

add_subdirectory(src)
add_subdirectory(tests)