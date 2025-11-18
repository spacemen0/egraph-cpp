cmake_minimum_required(VERSION 3.15)
project(egraph-cpp LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.0.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)

add_subdirectory(src)
add_subdirectory(tests)