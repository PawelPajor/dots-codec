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
  add_executable(test_dots tests/test_dots.c)
  target_link_libraries(test_dots PRIVATE dots)
  set_target_properties(test_dots PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(test_dots PRIVATE /W4 /WX)
  else()
    target_compile_options(test_dots PRIVATE -Wall -Wextra -pedantic -Werror)
  endif()
  add_test(NAME test_dots COMMAND test_dots)
endif()
