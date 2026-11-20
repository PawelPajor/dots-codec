<!--
SPDX-License-Identifier: MIT OR Apache-2.0
Copyright (c) 2026 Pawel Pajor
-->

# Contributing to dots-codec

What DOTS is, and how it changes, is decided in
[dots-stream](https://github.com/PawelPajor/dots-stream).

## 1. The specification comes first

Where a port and the specification differ, the port is fixed. A case
the specification does not settle goes to dots-stream as an issue.

## 2. Proposing a change

1. Beyond a small fix, open an issue first.
2. Make the change on a branch, one change per branch.
3. Keep every port passing the examples of the specification.
4. Open a pull request. The maintainer accepts or declines it, giving
   the reason.

## 3. Adding a port

A port is a top-level directory, named after its language: a native
implementation, without dependencies beyond the language's standard
library.

## 4. Code style

C and C++ code is formatted with clang-format and checked with
clang-tidy, as `.clang-format` and `.clang-tidy` set. To check
formatting before each commit, run `git config core.hooksPath .githooks`
once.

## 5. Commit messages

Commit messages follow
[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/),
with the subject in the imperative mood. The scope is the port
directory, and the type sets the version of that port:

| Type    | Use                       | Version |
|---------|---------------------------|---------|
| `feat`  | New capability            | `minor` |
| `fix`   | Correction                | `patch` |
| `docs`  | Documentation             | none    |
| `test`  | Tests only                | none    |
| `chore` | Tooling, CI, housekeeping | none    |

A breaking change is marked with `!` after the type and raises
`major`, or `minor` while `major` is 0.

## 6. Licence of contributions

Unless you explicitly state otherwise, any contribution you
intentionally submit for inclusion in this repository, as defined in
the Apache-2.0 licence, is dual-licensed under `MIT OR Apache-2.0`,
without any additional terms or conditions.
