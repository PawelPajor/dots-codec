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

## Examples

One program per level and direction, in [`examples/`](examples/),
built with the project:

| Example                                   | Does                                                |
|-------------------------------------------|-----------------------------------------------------|
| [`model_read.c`](examples/model_read.c)   | reads a stream and prints each frame                |
| [`model_write.c`](examples/model_write.c) | writes a stream of three frames                     |
| [`event_read.c`](examples/event_read.c)   | `model_read.c` at the event level                   |
| [`event_write.c`](examples/event_write.c) | `model_write.c` at the event level: the same octets |

```sh
build/examples/example_model_write hi.dots  # write a stream
build/examples/example_model_read hi.dots   # print its frames
```

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
ctest --test-dir build -R '^example/'    # the examples
ctest --test-dir build -N                # list the tests
```

The port is correct when every test passes.

- Acceptance tests: one per case of the
  [validation suite](../spec/validation-suite/README.md),
  `acceptance/<accept|reject>/<case>`, made when configuring.
- Unit tests: one per `SUITE` of `tests/test_unit.c` and language,
  `unit/<suite>` and `unit-cpp/<suite>`.
- Examples: each writer writes its stream, both give the same octets,
  and each reader reads both and the spec's
  [full example](../spec/examples/full.txt): `example/<name>`.

To run them without CTest, for instance under a debugger:

```sh
build/test_acceptance                                         # all acceptance tests
build/test_acceptance -e -t acceptance_<accept|reject>/<case> # one case
build/test_unit                                               # all unit tests, in C
build/test_unit -e -s <suite>                                 # one suite
build/test_unit_cpp                                           # all unit tests, in C++
```

---

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/PawelPajor/dots-stream/main/logo/dots-pixels-dark.png">
  <img alt="Logo" src="https://raw.githubusercontent.com/PawelPajor/dots-stream/main/logo/dots-pixels-light.png">
</picture>
