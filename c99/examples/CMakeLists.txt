# SPDX-License-Identifier: MIT OR Apache-2.0
# Copyright (c) 2026 Pawel Pajor

# The examples: one program per level and direction.
set(DOTS_EXAMPLES model_read model_write event_read event_write)
foreach(name IN LISTS DOTS_EXAMPLES)
  add_executable(example_${name} ${name}.c)
  dots_strict_c99(example_${name})
endforeach()

if(BUILD_TESTING)
  # Each writer writes its stream; both give the same octets.
  foreach(level model event)
    add_test(NAME example/${level}_write
      COMMAND example_${level}_write ${CMAKE_CURRENT_BINARY_DIR}/${level}.dots)
    set_tests_properties(example/${level}_write PROPERTIES
      FIXTURES_SETUP example_streams)
  endforeach()
  add_test(NAME example/same_octets
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${CMAKE_CURRENT_BINARY_DIR}/model.dots
      ${CMAKE_CURRENT_BINARY_DIR}/event.dots)
  set_tests_properties(example/same_octets PROPERTIES
    FIXTURES_REQUIRED example_streams)

  # Each reader reads both streams and the spec's full example.
  set(DOTS_FULL ${CMAKE_CURRENT_SOURCE_DIR}/../../spec/examples/full.dots)
  foreach(level model event)
    foreach(stream model event)
      add_test(NAME example/${level}_read/${stream}
        COMMAND example_${level}_read ${CMAKE_CURRENT_BINARY_DIR}/${stream}.dots)
      set_tests_properties(example/${level}_read/${stream} PROPERTIES
        FIXTURES_REQUIRED example_streams)
    endforeach()
    add_test(NAME example/${level}_read/full
      COMMAND example_${level}_read ${DOTS_FULL})
  endforeach()
endif()
