# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

cmake_minimum_required(VERSION 3.21)
project(dots_stream_c99 VERSION 0.2.0 LANGUAGES C)

include(GNUInstallDirs)

add_library(dots_c99 INTERFACE)
add_library(dots::c99 ALIAS dots_c99)
set_target_properties(dots_c99 PROPERTIES EXPORT_NAME c99)
target_include_directories(dots_c99 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  include(CMakePackageConfigHelpers)

  set(DOTS_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/cmake/dots_stream_c99)
  install(TARGETS dots_c99 EXPORT dots_stream_c99)
  install(FILES dots.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT dots_stream_c99 NAMESPACE dots::
    FILE dots_stream_c99Config.cmake DESTINATION ${DOTS_CMAKE_DIR})
  write_basic_package_version_file(dots_stream_c99ConfigVersion.cmake
    COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dots_stream_c99ConfigVersion.cmake
    DESTINATION ${DOTS_CMAKE_DIR})

  # The documentation of dots.h, built by the docs target.
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_EXCLUDE_SYMBOLS dots_detail_* DOTS_DETAIL_* *_prv prv)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
    set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
    set(DOXYGEN_GENERATE_LATEX NO)
    # The logos of the specification; the main page picks the large one
    # matching the theme.
    set(DOTS_LOGO ${CMAKE_CURRENT_SOURCE_DIR}/../spec/logo)
    set(DOXYGEN_PROJECT_LOGO ${DOTS_LOGO}/dots-pixels-dark.png)
    set(DOXYGEN_HTML_EXTRA_FILES
      ${DOTS_LOGO}/dots-braille-dark.png ${DOTS_LOGO}/dots-braille-light.png)
    doxygen_add_docs(docs dots.h)
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  # greatest (https://github.com/silentbicycle/greatest), v1.5.0.
  include(FetchContent)
  FetchContent_Declare(greatest
    URL https://github.com/silentbicycle/greatest/archive/11a6af1919049df502405913da64fb385c219361.tar.gz
    URL_HASH SHA256=e1ddc85dc4892318993e0dee0acf891a123eeff9748c549cc19d5387d73b98e1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(greatest)

  # tomlc17 (https://github.com/cktan/tomlc17), R261116, to read the
  # .meta files of the validation suite.
  FetchContent_Declare(tomlc17
    URL https://github.com/cktan/tomlc17/archive/bfc5b9cd953daa07d095dc7e893954b2115ee8e0.tar.gz
    URL_HASH SHA256=67b60972577bef40a8cace9e473b2c0f4bcac18503bed57c4686e98936e6e599
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(tomlc17)
  add_library(tomlc17 STATIC ${tomlc17_SOURCE_DIR}/src/tomlc17.c)
  target_include_directories(tomlc17 PUBLIC ${tomlc17_SOURCE_DIR}/src)
  set_target_properties(tomlc17 PROPERTIES C_STANDARD 99)
  if(MSVC)
    target_compile_definitions(tomlc17 PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  # A test program in strict C99, with warnings as errors.
  function(dots_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE dots::c99)
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

  dots_test(test_unit)

  # The same tests in C++, where dots.h compiles too.
  enable_language(CXX)
  add_executable(test_unit_cpp tests/test_unit_cpp.cpp)
  target_link_libraries(test_unit_cpp PRIVATE dots::c99)
  target_include_directories(test_unit_cpp PRIVATE ${greatest_SOURCE_DIR})
  set_target_properties(test_unit_cpp PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  if(MSVC)
    # C4551: greatest marks its functions used with (void)name.
    target_compile_options(test_unit_cpp PRIVATE /W4 /WX /wd4551)
    target_compile_definitions(test_unit_cpp PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(test_unit_cpp PRIVATE -Wall -Wextra -pedantic -Werror)
  endif()

  # The unit tests: one test per SUITE of tests/test_unit.c and
  # language. A name that matches no suite passes silently in greatest,
  # so each must report a pass.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    tests/test_unit.c)
  file(STRINGS tests/test_unit.c DOTS_UNIT_SUITES
    REGEX "^SUITE\\([a-z0-9_]+\\)")
  foreach(suite IN LISTS DOTS_UNIT_SUITES)
    string(REGEX REPLACE "^SUITE\\(([a-z0-9_]+)\\).*" "\\1" suite "${suite}")
    add_test(NAME unit/${suite} COMMAND test_unit -e -s ${suite})
    add_test(NAME unit-cpp/${suite} COMMAND test_unit_cpp -e -s ${suite})
    set_tests_properties(unit/${suite} unit-cpp/${suite} PROPERTIES
      PASS_REGULAR_EXPRESSION "Pass: [1-9][0-9]*, fail: 0")
  endforeach()

  # The acceptance tests: one test per case of the validation suite.
  # A name that matches no test passes silently in greatest, so each
  # must report one pass.
  set(DOTS_SUITE ${CMAKE_CURRENT_SOURCE_DIR}/../spec/validation-suite)
  if(NOT EXISTS ${DOTS_SUITE}/README.md)
    message(FATAL_ERROR "spec/ is empty: run git submodule update --init")
  endif()
  file(GLOB DOTS_METAS CONFIGURE_DEPENDS ${DOTS_SUITE}/*/*.meta)
  set(DOTS_CASES "")
  foreach(meta IN LISTS DOTS_METAS)
    file(RELATIVE_PATH case ${DOTS_SUITE} ${meta})
    string(REGEX REPLACE "\\.meta$" "" case ${case})
    string(APPEND DOTS_CASES "  \"${case}\",\n")
    add_test(NAME acceptance/${case}
      COMMAND test_acceptance -e -t acceptance_${case})
    set_tests_properties(acceptance/${case} PROPERTIES
      PASS_REGULAR_EXPRESSION "Pass: 1, fail: 0")
  endforeach()
  file(CONFIGURE OUTPUT suite.h CONTENT [[
#define DOTS_SUITE_DIR "@DOTS_SUITE@"
static const char *const cases[] = {
@DOTS_CASES@};
]] @ONLY)
  dots_test(test_acceptance)
  target_include_directories(test_acceptance
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(test_acceptance PRIVATE tomlc17)
endif()
