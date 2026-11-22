<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# cpp23

C++23 port: header-only, standard library only, written from the specification.

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
