cmake_minimum_required(VERSION 3.12)

project(csjp VERSION 2.0.0 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CSJP_IS_TOP_LEVEL ON)
else()
  set(CSJP_IS_TOP_LEVEL OFF)
endif()

include(CMakePackageConfigHelpers)

option(CSJP_BUILD_STATIC "Build a compiled static library target" ON)
option(CSJP_BUILD_TESTS "Build tests" ${CSJP_IS_TOP_LEVEL})
option(CSJP_BUILD_BENCHMARKS "Build benchmarks" ${CSJP_IS_TOP_LEVEL})

add_library(csjp INTERFACE)
target_include_directories(csjp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)
add_library(csjp::csjp ALIAS csjp)

if(CSJP_BUILD_STATIC)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/csjp_impl.c"
       "#define JSON_IMPLEMENTATION\n#include \"json.h\"\n")
  add_library(csjp_static STATIC "${CMAKE_CURRENT_BINARY_DIR}/csjp_impl.c")
  target_include_directories(csjp_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)
  set_target_properties(csjp_static PROPERTIES OUTPUT_NAME csjp)
  add_library(csjp::csjp_static ALIAS csjp_static)
endif()

if(CSJP_BUILD_TESTS)
  enable_testing()
  if(CSJP_BUILD_STATIC)
    add_executable(csjp_tests tests/test_json.c)
    target_link_libraries(csjp_tests PRIVATE csjp_static)
  else()
    add_executable(csjp_tests tests/test_json.c json.c)
    target_link_libraries(csjp_tests PRIVATE csjp)
  endif()
  add_test(NAME csjp_tests COMMAND csjp_tests)
endif()

if(CSJP_BUILD_BENCHMARKS)
  if(CSJP_BUILD_STATIC)
    add_executable(csjp_bench benchmarks/bench_parse.c)
    target_link_libraries(csjp_bench PRIVATE csjp_static)
  else()
    add_executable(csjp_bench benchmarks/bench_parse.c json.c)
    target_link_libraries(csjp_bench PRIVATE csjp)
  endif()
endif()

install(FILES json.h DESTINATION include)
install(TARGETS csjp EXPORT csjpTargets)
if(CSJP_BUILD_STATIC)
  install(TARGETS csjp_static EXPORT csjpTargets ARCHIVE DESTINATION lib)
endif()
install(EXPORT csjpTargets NAMESPACE csjp:: DESTINATION lib/cmake/csjp)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/csjpConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/csjpConfig.cmake"
  INSTALL_DESTINATION lib/cmake/csjp)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/csjpConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/csjpConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/csjpConfigVersion.cmake"
  DESTINATION lib/cmake/csjp)
