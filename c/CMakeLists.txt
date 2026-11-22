# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

cmake_minimum_required(VERSION 3.21)
project(dots_stream VERSION 0.0.0 LANGUAGES C)

include(GNUInstallDirs)

add_library(dots INTERFACE)
add_library(dots::dots ALIAS dots)
target_include_directories(dots INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  include(CMakePackageConfigHelpers)

  set(DOTS_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/cmake/dots_stream)
  install(TARGETS dots EXPORT dots_stream)
  install(FILES dots.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT dots_stream NAMESPACE dots::
    FILE dots_streamConfig.cmake DESTINATION ${DOTS_CMAKE_DIR})
  write_basic_package_version_file(dots_streamConfigVersion.cmake
    COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dots_streamConfigVersion.cmake
    DESTINATION ${DOTS_CMAKE_DIR})
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  # A test program in strict C99, with warnings as errors.
  function(dots_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE dots)
    set_target_properties(${name} PROPERTIES
      C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(MSVC)
      target_compile_options(${name} PRIVATE /W4 /WX)
      target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
      target_compile_options(${name} PRIVATE -Wall -Wextra -pedantic -Werror)
    endif()
  endfunction()

  dots_test(test_dots)
  add_test(NAME test_dots COMMAND test_dots)

  # One test per example of the specification.
  set(DOTS_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/../spec/examples)
  if(NOT EXISTS ${DOTS_EXAMPLES}/README.md)
    message(FATAL_ERROR "spec/ is empty: run git submodule update --init")
  endif()
  dots_test(test_examples)
  file(GLOB DOTS_LISTINGS CONFIGURE_DEPENDS ${DOTS_EXAMPLES}/*/*.txt)
  foreach(listing IN LISTS DOTS_LISTINGS)
    string(REGEX REPLACE "\\.txt$" ".dots" stream ${listing})
    file(RELATIVE_PATH name ${DOTS_EXAMPLES} ${listing})
    string(REGEX REPLACE "\\.txt$" "" name ${name})
    add_test(NAME example/${name} COMMAND test_examples ${listing} ${stream})
  endforeach()
endif()
