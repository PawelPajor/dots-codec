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
  # greatest (https://github.com/silentbicycle/greatest), v1.5.0.
  include(FetchContent)
  FetchContent_Declare(greatest
    URL https://github.com/silentbicycle/greatest/archive/11a6af1919049df502405913da64fb385c219361.tar.gz
    URL_HASH SHA256=e1ddc85dc4892318993e0dee0acf891a123eeff9748c549cc19d5387d73b98e1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(greatest)

  # A test program in strict C99, with warnings as errors.
  function(dots_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE dots)
    target_include_directories(${name} PRIVATE ${greatest_SOURCE_DIR})
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
endif()
