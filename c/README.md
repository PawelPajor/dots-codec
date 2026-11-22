<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# c

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

The tests use [greatest](https://github.com/silentbicycle/greatest),
which CMake fetches when it configures them; the library itself does
not depend on it. To configure without network access, after a first
fetch, add `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; to build without
the tests, add `-DBUILD_TESTING=OFF`.
