cmake_minimum_required(VERSION 3.15)
project(egraph-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# if (APPLE AND EXISTS "/opt/homebrew/opt/llvm/bin/clang++")
#     message(STATUS "macOS Homebrew LLVM detected. Adjusting linker settings.")

#     link_directories(/opt/homebrew/opt/llvm/lib)

#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/opt/homebrew/opt/llvm/lib")
# endif()

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