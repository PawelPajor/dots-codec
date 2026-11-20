/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * dots.h: reader and writer of DOTS streams, in C99.
 *
 * Include this header wherever it is needed. In exactly one source
 * file, define DOTS_IMPLEMENTATION before including it.
 */

#ifndef DOTS_H
#define DOTS_H

/* The version of the specification this header implements. */
#define DOTS_SPEC_MAJOR 0
#define DOTS_SPEC_MINOR 1
#define DOTS_SPEC_UPDATE 1

/* Result codes, with the values of the specification. */
enum dots_code {
  DOTS_OK = 0,
  DOTS_UNSUPPORTED_VERSION = 1,
  DOTS_INVALID_VALUE = 2,
  DOTS_UNEXPECTED_TEXT = 3,
  DOTS_OUTSIDE_GRID = 4,
  DOTS_TRUNCATED = 5,
  DOTS_MALFORMED = 6,
  DOTS_OVERFLOW = 7,
  DOTS_INVALID_TEXT = 8
};

/* The name of a code, as the specification writes it, or 0 if unknown. */
const char *dots_code_name(enum dots_code code);

#endif /* DOTS_H */

#if defined(DOTS_IMPLEMENTATION) && !defined(DOTS_IMPLEMENTATION_DONE)
#define DOTS_IMPLEMENTATION_DONE

const char *dots_code_name(enum dots_code code) {
  switch (code) {
  case DOTS_OK:
    return "OK";
  case DOTS_UNSUPPORTED_VERSION:
    return "UNSUPPORTED_VERSION";
  case DOTS_INVALID_VALUE:
    return "INVALID_VALUE";
  case DOTS_UNEXPECTED_TEXT:
    return "UNEXPECTED_TEXT";
  case DOTS_OUTSIDE_GRID:
    return "OUTSIDE_GRID";
  case DOTS_TRUNCATED:
    return "TRUNCATED";
  case DOTS_MALFORMED:
    return "MALFORMED";
  case DOTS_OVERFLOW:
    return "OVERFLOW";
  case DOTS_INVALID_TEXT:
    return "INVALID_TEXT";
  }
  return 0;
}

#endif /* DOTS_IMPLEMENTATION */
