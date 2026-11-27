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
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
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

  dots_test(test_dots)
  add_test(NAME test_dots COMMAND test_dots)

  # The acceptance tests: one test per case of the validation suite.
  set(DOTS_SUITE ${CMAKE_CURRENT_SOURCE_DIR}/../spec/validation-suite)
  dots_test(test_acceptance)
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
