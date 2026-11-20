# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

cmake_minimum_required(VERSION 3.21)
project(dots_stream_cpp VERSION 0.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(dots_cpp INTERFACE)
add_library(dots::cpp ALIAS dots_cpp)
set_target_properties(dots_cpp PROPERTIES EXPORT_NAME cpp)
target_compile_features(dots_cpp INTERFACE cxx_std_23)
target_include_directories(dots_cpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  include(CMakePackageConfigHelpers)

  set(DOTS_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/cmake/dots_stream_cpp)
  install(TARGETS dots_cpp EXPORT dots_stream_cpp)
  install(FILES dots.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT dots_stream_cpp NAMESPACE dots::
    FILE dots_stream_cppConfig.cmake DESTINATION ${DOTS_CMAKE_DIR})
  write_basic_package_version_file(dots_stream_cppConfigVersion.cmake
    COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dots_stream_cppConfigVersion.cmake
    DESTINATION ${DOTS_CMAKE_DIR})
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  add_executable(test_dots_cpp tests/test_dots.cpp)
  target_link_libraries(test_dots_cpp PRIVATE dots::cpp)
  set_target_properties(test_dots_cpp PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(test_dots_cpp PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(test_dots_cpp PRIVATE -Wall -Wextra -pedantic -Werror)
  endif()
  add_test(NAME test_dots_cpp COMMAND test_dots_cpp)
endif()
