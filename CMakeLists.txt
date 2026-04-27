cmake_minimum_required(VERSION 3.15)
project(egraph-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# Configure build settings before adding dependencies
option(ENABLE_COVERAGE "Build with gcov/coverage instrumentation" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(ENABLE_COVERAGE)
        message(STATUS "Building with coverage instrumentation")
        add_compile_options(--coverage -g)
        add_link_options(--coverage)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Building with AddressSanitizer")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        if(WIN32)
            # Link against ASan runtime on Windows requires _ITERATOR_DEBUG_LEVEL=0
            # and matching CRT (usually static/dynamic release CRT)
            add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
        endif()
    else()
        message(STATUS "Release mode: Building with optimizations")
        add_compile_options(-O3)
    endif()
elseif(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "Release mode: Building with optimizations")
        add_compile_options(/O2)
    endif()
endif()

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

add_subdirectory(src)
add_subdirectory(tests)
