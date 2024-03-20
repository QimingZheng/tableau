cmake_minimum_required(VERSION 3.24)

project(
  tableau
  VERSION 1.0
  LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main gmock_main)

file(GLOB HEADERS *.h)
file(GLOB TEST_SRCS *_test.cc)

include_directories(headers ${HEADERS})

enable_testing()

foreach(test_src ${TEST_SRCS})
  string(FIND ${test_src} "/" start REVERSE)
  string(FIND ${test_src} ".cc" end)
  MATH(EXPR begin "${start}+1")
  MATH(EXPR length "${end}-${start}-1")
  string(SUBSTRING ${test_src} ${begin} ${length} test_name)
  add_executable(
    ${test_name}
    ${test_src}
  )
  target_include_directories(${test_name} PRIVATE headers)
  target_link_libraries(
    ${test_name}
    GTest::GTest
  )
  add_test(
    NAME ${test_name}
    COMMAND ${test_name}
  )
endforeach()
