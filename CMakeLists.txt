cmake_minimum_required(VERSION 3.24)

project(
  tableau
  VERSION 1.0
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
SET(GCC_COVERAGE_COMPILE_FLAGS "-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
FetchContent_MakeAvailable(
  googletest
  googlebenchmark
)

add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main gmock_main)

file(GLOB HEADERS *.h)
file(GLOB TEST_SRCS *_test.cc)

include_directories(headers ${HEADERS})

add_executable(tableau_benchmark tableau_benchmark.cc)
target_link_libraries(tableau_benchmark benchmark::benchmark tcmalloc)

enable_testing()

add_executable(
  tableau_test
  tableau_test.cc
)
target_include_directories(tableau_test PRIVATE headers)
target_link_libraries(
  tableau_test
  GTest::GTest
)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(tableau_test OpenMP::OpenMP_CXX)
endif()
add_test(
  NAME tableau_test
  COMMAND tableau_test
)

add_test(
  NAME tableau_benchmark
  COMMAND tableau_benchmark
)
