# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

cmake_minimum_required(VERSION 3.21)
project(dots_stream_cpp23 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(dots_cpp23 INTERFACE)
add_library(dots::cpp23 ALIAS dots_cpp23)
set_target_properties(dots_cpp23 PROPERTIES EXPORT_NAME cpp23)
target_compile_features(dots_cpp23 INTERFACE cxx_std_23)
target_include_directories(dots_cpp23 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  include(CMakePackageConfigHelpers)

  set(DOTS_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/cmake/dots_stream_cpp23)
  install(TARGETS dots_cpp23 EXPORT dots_stream_cpp23)
  install(FILES dots.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT dots_stream_cpp23 NAMESPACE dots::
    FILE dots_stream_cpp23Config.cmake DESTINATION ${DOTS_CMAKE_DIR})
  write_basic_package_version_file(dots_stream_cpp23ConfigVersion.cmake
    COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dots_stream_cpp23ConfigVersion.cmake
    DESTINATION ${DOTS_CMAKE_DIR})

  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_EXCLUDE_SYMBOLS dots::detail)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
    set(DOXYGEN_GENERATE_LATEX NO)
    doxygen_add_docs(docs dots.hpp)
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(FetchContent)

  # Catch2 (https://github.com/catchorg/Catch2), v3.16.1.
  FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/08092139210881f01a34e60c5f2fd6afa0ea4024.tar.gz
    URL_HASH SHA256=c66daf9f31712f673ebdffb074c90b5d7e61ea50adc6a1d2db95e798af027940
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

  # toml++ (https://github.com/marzer/tomlplusplus), v3.5.1, to read the
  # .meta files of the validation suite.
  FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/efd80e03356af65c7d4e61c56ffb326b34f8d0f4.tar.gz
    URL_HASH SHA256=036275301932ec0991885c59d38e8850a119a219b9dc543101432bbcc1f21207
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(Catch2 tomlplusplus)
  include(${catch2_SOURCE_DIR}/extras/Catch.cmake)

  function(dots_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE dots::cpp23)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
      target_compile_options(${name} PRIVATE /W4 /WX /permissive-)
    else()
      target_compile_options(${name} PRIVATE -Wall -Wextra -pedantic -Werror)
    endif()
  endfunction()

  # The unit tests: one test per test case.
  dots_test(test_dots)
  target_link_libraries(test_dots PRIVATE Catch2::Catch2WithMain)
  catch_discover_tests(test_dots TEST_PREFIX unit/)

  # The acceptance tests: one test per case of the validation suite.
  set(DOTS_SUITE ${CMAKE_CURRENT_SOURCE_DIR}/../spec/validation-suite)
  dots_test(test_acceptance)
  target_link_libraries(test_acceptance PRIVATE tomlplusplus::tomlplusplus)
  file(GLOB DOTS_METAS CONFIGURE_DEPENDS ${DOTS_SUITE}/*/*.meta)
  foreach(meta IN LISTS DOTS_METAS)
    file(RELATIVE_PATH case ${DOTS_SUITE} ${meta})
    string(REGEX REPLACE "\\.meta$" "" case "${case}")
    add_test(NAME acceptance/${case}
      COMMAND test_acceptance ${DOTS_SUITE} ${case})
  endforeach()

  # The examples of the specification, accepted and written again.
  set(DOTS_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/../spec/examples)
  file(GLOB DOTS_STREAMS CONFIGURE_DEPENDS ${DOTS_EXAMPLES}/*.dots)
  foreach(stream IN LISTS DOTS_STREAMS)
    get_filename_component(name ${stream} NAME_WE)
    add_test(NAME example/${name}
      COMMAND test_acceptance ${DOTS_EXAMPLES} ${name})
  endforeach()
endif()
