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

There is one test per example of the specification. The port is
correct when every test passes; a failing example is a point where the
port and the specification disagree.

To work on the port, one example at a time:

1. Pick a failing example and read its listing,
   `spec/examples/<accept|reject>/<name>.txt`: its first comment says
   what a reader returns, and why.
2. Run its test alone until it passes:
   ```sh
   ctest --test-dir build -R example/reject/hdr-1-version
   build/test_examples -e -t example_reject/hdr-1-version   # same, under a debugger
   ```
3. Run all the tests.
