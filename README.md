<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# dots-codec

Official readers and writers of DOTS streams, as specified in
[dots-stream](https://github.com/PawelPajor/dots-stream), in many
languages.

Each port is a native implementation in its language, with no
dependencies and no generated code. Each port tests itself against
the examples of the specification, which state what a reader returns
for every stream: `OK`, or an error code and offset. Where a port
and the specification differ, the specification is right and the
port is fixed.

## Ports

| Directory       | Language | Status      | CI                                                                                                                                                                           |
|-----------------|----------|-------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [c99](c99/)     | C99      | In progress | [![c99](https://github.com/PawelPajor/dots-codec/actions/workflows/c99.yml/badge.svg?branch=main)](https://github.com/PawelPajor/dots-codec/actions/workflows/c99.yml)       |
| [cpp23](cpp23/) | C++23    | Planned     | [![cpp23](https://github.com/PawelPajor/dots-codec/actions/workflows/cpp23.yml/badge.svg?branch=main)](https://github.com/PawelPajor/dots-codec/actions/workflows/cpp23.yml) |

More languages follow, in order of demand. Each port has its own version
and release tags.

Other implementations of DOTS are welcome. Anyone may implement DOTS
without asking; see
[GOVERNANCE.md](https://github.com/PawelPajor/dots-stream/blob/main/GOVERNANCE.md)
in the specification.

## Repository layout

```
spec/        the specification (dots-stream), pinned to a tag
c99/         C99 port
cpp23/       C++23 port
fuzz/        differential fuzzing across ports
```

Clone with `git clone --recurse-submodules`, or run
`git submodule update --init` after cloning.

## Contributing and security

[CONTRIBUTING.md](CONTRIBUTING.md) says how changes are made.
Report vulnerabilities privately, as [SECURITY.md](SECURITY.md)
describes.

## Licence

The code is licensed under either of

- MIT licence ([LICENSE-MIT](LICENSE-MIT))
- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))

at your option (`MIT OR Apache-2.0`).

The specification under `spec/`, including its examples, stays under
CC BY 4.0, its own licence.

Unless you explicitly state otherwise, any contribution you
intentionally submit for inclusion in this repository, as defined in
the Apache-2.0 licence, is dual-licensed as above, without any
additional terms or conditions.
