<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# c99

C99 port: a single header, `dots.h`, with no dependencies and no allocation required.

## Building

```sh
cmake -S . -B build                     # configure
cmake --build build                     # build
cmake --install build --prefix install  # install
cmake -S tests/package -B build-package -DCMAKE_PREFIX_PATH="$PWD/install" \
  && cmake --build build-package && ctest --test-dir build-package  # package check
```

## Documentation

The documentation of `dots.h`, from its Doxygen comments, is at
<https://pawelpajor.github.io/dots-codec/c99/>, built from `main`.

With [Doxygen](https://www.doxygen.nl) installed, `cmake --build build --target docs`
writes the documentation of `dots.h` to `build/html/index.html`. A
documentation error, such as a `@param` naming no parameter, fails
the build.

## Tests

The tests use [greatest](https://github.com/silentbicycle/greatest)
and [tomlc17](https://github.com/cktan/tomlc17), which CMake fetches
when it configures them; the library itself does not depend on them.
To configure without network access, after a first fetch, add
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`.

```sh
ctest --test-dir build                   # all tests
ctest --test-dir build -R '^acceptance/' # the acceptance tests
ctest --test-dir build -R '^unit'        # the unit tests, in C and C++
ctest --test-dir build -N                # list the tests
```

The port is correct when every test passes.

- Acceptance tests: one per case of the
  [validation suite](../spec/validation-suite/README.md),
  `acceptance/<accept|reject>/<case>`, made when configuring.
- Unit tests: one per `SUITE` of `tests/test_unit.c` and language,
  `unit/<suite>` and `unit-cpp/<suite>`.

To run them without CTest, for instance under a debugger:

```sh
build/test_acceptance                                         # all acceptance tests
build/test_acceptance -e -t acceptance_<accept|reject>/<case> # one case
build/test_unit                                               # all unit tests, in C
build/test_unit -e -s <suite>                                 # one suite
build/test_unit_cpp                                           # all unit tests, in C++
```
