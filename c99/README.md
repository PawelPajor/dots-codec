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
build/test_dots                         # run
ctest --test-dir build                  # test
cmake --install build --prefix install  # install
cmake -S tests/package -B build-package -DCMAKE_PREFIX_PATH="$PWD/install" \
  && cmake --build build-package && ctest --test-dir build-package  # package check
```

## Tests

The tests use [greatest](https://github.com/silentbicycle/greatest)
and [tomlc17](https://github.com/cktan/tomlc17), which CMake fetches
when it configures them; the library itself does not depend on them.
To configure without network access, after a first fetch, add
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; to build without the tests,
add `-DBUILD_TESTING=OFF`.

There is one test per case of the
[validation suite](../spec/validation-suite/README.md). The port is
correct when every test passes. To run one case, alone or under a
debugger:

```sh
ctest --test-dir build -R suite/reject/zero-columns
build/test_suite -e -t suite_reject/zero-columns
```
