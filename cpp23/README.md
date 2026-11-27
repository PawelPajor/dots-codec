<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# cpp23

C++23 port: a single header, `dots.hpp`, standard library only, written from the specification.
It needs `std::expected`: GCC 13, or Clang 19 with libstdc++, or later.

The unit is a frame. A `decoder` reads frames one at a time, an
`encoder` writes them, and a `composer` applies them to the state of
the grid. Nothing throws: every failure is a `dots::error` in a
`std::expected`.

## Reading

```cpp
dots::decoder decoder;
std::span<const std::byte> input = ...; // any piece of the stream

while (!input.empty()) {
  auto fed = decoder.feed(input);

  if (!fed) {
    // fed.error(): its code and offset
  }
  if (fed->completed) {
    // *fed->completed: the next frame
  }
  input = input.subspan(fed->used);
}
// ... more pieces, then:
auto end = decoder.finish(); // TRUNCATED if the stream has not ended
```

`decoder.stream_header()` holds the header once it is read. To show
the frames, apply each to a composer:

```cpp
auto composer = dots::composer::create(*decoder.stream_header());

composer->apply(frame);
composer->cells(); // cell (x, y) at y * columns + x
```

## Writing

```cpp
std::vector<std::byte> out;
dots::encoder encoder;
const dots::header header = {{0, 1, 0}, 1, {16, 4}, 10}; // contains-text

encoder.start(header, std::back_inserter(out));
encoder.put({true, {}, {{{0, 0}, "Hi"}}}, std::back_inserter(out));
encoder.finish(std::back_inserter(out)); // end-of-stream
```

Each call checks its input before writing: a refused call writes
nothing.

## Building

```sh
cmake -S . -B build                     # configure
cmake --build build                     # build
ctest --test-dir build                  # test
cmake --install build --prefix install  # install
cmake -S tests/package -B build-package -DCMAKE_PREFIX_PATH="$PWD/install" \
  && cmake --build build-package && ctest --test-dir build-package  # package check
```

## Documentation

With [Doxygen](https://www.doxygen.nl) installed, `cmake --build build --target docs`
writes the documentation of `dots.hpp` to `build/html/index.html`. A
documentation error, such as a `@param` naming no parameter, fails
the build.

## Tests

The tests use [Catch2](https://github.com/catchorg/Catch2) and
[toml++](https://github.com/marzer/tomlplusplus), which CMake fetches
when it configures them; the library itself does not depend on them.

```sh
ctest --test-dir build -R '^acceptance/' # one test per case of the validation suite
ctest --test-dir build -R '^unit/'       # one test per test case of tests/test_dots.cpp
```

---

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/PawelPajor/dots-stream/main/logo/dots-pixels-dark.png">
  <img alt="Logo" src="https://raw.githubusercontent.com/PawelPajor/dots-stream/main/logo/dots-pixels-light.png">
</picture>
