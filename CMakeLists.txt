cmake_minimum_required(VERSION 3.12)
project(csjp VERSION 3.0.0 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CSJP_STANDALONE ON)
else()
  set(CSJP_STANDALONE OFF)
endif()

option(CSJP_BUILD_TESTS "Build tests" ${CSJP_STANDALONE})
option(CSJP_BUILD_BENCHMARKS "Build benchmarks" ${CSJP_STANDALONE})

add_library(csjp INTERFACE)
target_include_directories(csjp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(csjp::csjp ALIAS csjp)

if(CSJP_BUILD_TESTS)
  enable_testing()
  add_executable(csjp_tests tests/test_json.c)
  target_include_directories(csjp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME csjp_tests COMMAND csjp_tests)
endif()

if(CSJP_BUILD_BENCHMARKS)
  add_executable(csjp_bench benchmarks/bench_parse.c)
  target_include_directories(csjp_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

install(FILES json.h DESTINATION include)
