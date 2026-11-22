/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 */

#define DOTS_IMPLEMENTATION
#include "dots.h"

#include "greatest.h"

TEST code_names(void) {
  ASSERT_STR_EQ("OK", dots_code_name(DOTS_OK));
  ASSERT_STR_EQ("INVALID_TEXT", dots_code_name(DOTS_INVALID_TEXT));
  ASSERT_EQ(0, dots_code_name((enum dots_code)9));
  PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_TEST(code_names);
  GREATEST_MAIN_END();
}
