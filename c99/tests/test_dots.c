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

/* A header with multi-octet values, frame-rate 2^64 - 1 among them. */
TEST header(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x85, 0x01, 0x01, 0xC8, 0x01, 0x04,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF};
  struct dots_reader reader;
  struct dots_event event;
  size_t used;

  dots_reader_init(&reader);
  used = dots_reader_feed(&reader, stream, sizeof stream, &event);
  ASSERT_EQ(sizeof stream - 1, used);
  ASSERT_EQ(DOTS_EVENT_HEADER, event.type);
  ASSERT_EQ(0, event.header.major);
  ASSERT_EQ(1, event.header.minor);
  ASSERT_EQ(133, event.header.update);
  ASSERT_EQ(1, event.header.flags);
  ASSERT_EQ(200, event.header.columns);
  ASSERT_EQ(4, event.header.rows);
  ASSERT_EQ(UINT64_MAX, event.header.frame_rate);
  PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_TEST(code_names);
  RUN_TEST(header);
  GREATEST_MAIN_END();
}
