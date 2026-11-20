/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 */

#define DOTS_IMPLEMENTATION
#include "dots.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

int main(void) {
  check(DOTS_SPEC_MAJOR == 0 && DOTS_SPEC_MINOR == 1, "spec version");
  check(strcmp(dots_code_name(DOTS_OK), "OK") == 0, "name of OK");
  check(strcmp(dots_code_name(DOTS_INVALID_TEXT), "INVALID_TEXT") == 0,
        "name of INVALID_TEXT");
  check(dots_code_name((enum dots_code)9) == 0, "unknown code");
  return failures != 0;
}
