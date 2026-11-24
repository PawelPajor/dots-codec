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

/* The events of two frames, fed whole, then octets after the end. */
TEST events(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04,
      0x0A, 0x01, 0x02, 0x03, 0x01, 0x02, 0x39, 0x47, 0xFE, 0xFD,
      0xFE, 0x02, 0x00, 0x00, 0x00, 0xFD, 0xFF, 0x05};
  static const enum dots_event_type types[] = {
      DOTS_EVENT_HEADER,      DOTS_EVENT_FRAME,         DOTS_EVENT_RUN,
      DOTS_EVENT_CHUNK,       DOTS_EVENT_END_OF_BLOCKS, DOTS_EVENT_END_OF_TEXT,
      DOTS_EVENT_FRAME,       DOTS_EVENT_END_OF_BLOCKS, DOTS_EVENT_RUN,
      DOTS_EVENT_END_OF_TEXT, DOTS_EVENT_END_OF_STREAM, DOTS_EVENT_NONE};
  static const uint64_t offsets[] = {0,  11, 12, 16, 18, 19,
                                     20, 20, 21, 25, 26, 0};
  struct dots_reader reader;
  struct dots_event event;
  size_t at = 0;
  size_t i;

  dots_reader_init(&reader);
  for (i = 0; i < sizeof types / sizeof types[0]; ++i) {
    at += dots_reader_feed(&reader, stream + at, sizeof stream - at, &event);
    ASSERT_EQ(types[i], event.type);
    ASSERT_EQ(offsets[i], event.offset);
    if (i == 1) {
      ASSERT_EQ(1, event.clear);
    } else if (i == 2) {
      ASSERT_EQ(0, event.run.text);
      ASSERT_EQ(3, event.run.x);
      ASSERT_EQ(1, event.run.y);
      ASSERT_EQ(2, event.run.count);
    } else if (i == 3) {
      ASSERT_EQ(stream + 16, event.chunk.octets);
      ASSERT_EQ(2, event.chunk.size);
    } else if (i == 6) {
      ASSERT_EQ(0, event.clear);
    } else if (i == 8) {
      ASSERT_EQ(1, event.run.text);
      ASSERT_EQ(0, event.run.count);
    }
  }
  ASSERT_EQ(sizeof stream, at);
  dots_reader_finish(&reader, &event);
  ASSERT_EQ(DOTS_EVENT_END_OF_STREAM, event.type);
  ASSERT_EQ(26, event.offset);
  PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_TEST(code_names);
  RUN_TEST(header);
  RUN_TEST(events);
  GREATEST_MAIN_END();
}
