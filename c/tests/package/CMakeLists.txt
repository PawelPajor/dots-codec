# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

# A project that uses the installed package, as a user would.

cmake_minimum_required(VERSION 3.21)
project(dots_package_test LANGUAGES C)

find_package(dots_stream REQUIRED)

enable_testing()
add_executable(package_test main.c)
target_link_libraries(package_test PRIVATE dots::dots)
add_test(NAME package_test COMMAND package_test)
