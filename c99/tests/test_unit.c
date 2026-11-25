/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * The unit tests: one SUITE per unit, the reader and the internal
 * sub-APIs of dots.h.
 */

#define DOTS_IMPLEMENTATION
#include "dots.h"

#include "greatest.h"

#include <stdio.h>
#include <string.h>

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

/* A text run whose characters end in an error. */
struct text_error_case {
  const char *name;
  unsigned char stream[32];
  size_t size;
  unsigned char characters[4]; /* those before the error */
  size_t characters_size;
  enum dots_code code;
  uint64_t offset;
};

/* Each run's data starts at offset 16. */
static const struct text_error_case text_error_cases[] = {
    /* "A€", then E2 82 41 */
    {"invalid",
     {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10,
      0x04, 0x0A, 0xFE, 0x02, 0x00, 0x00, 0x07, 0x41, 0xE2,
      0x82, 0xAC, 0xE2, 0x82, 0x41, 0xFD, 0xFF},
     25,
     {0x41, 0xE2, 0x82, 0xAC},
     4,
     DOTS_INVALID_TEXT,
     20},
    /* "A€" at column 15 of 16: "€" is outside the grid */
    {"outside",
     {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04, 0x0A,
      0xFE, 0x02, 0x0F, 0x00, 0x04, 0x41, 0xE2, 0x82, 0xAC, 0xFD, 0xFF},
     22,
     {0x41},
     1,
     DOTS_OUTSIDE_GRID,
     17}};

/*
 * Feeds a text run that ends in an error, in chunks of every size: the
 * characters before the error come first, whatever the chunks cut.
 */
TEST text_error(const struct text_error_case *test) {
  static char message[32]; /* greatest prints it after the test returns */
  size_t chunk;

  for (chunk = 1; chunk <= test->size; ++chunk) {
    struct dots_reader reader;
    struct dots_event event;
    unsigned char text[sizeof test->stream];
    size_t text_size = 0;
    size_t at = 0;
    int failed = 0;

    snprintf(message, sizeof message, "in chunks of %lu", (unsigned long)chunk);
    dots_reader_init(&reader);
    while (at < test->size && !failed) {
      size_t end = at + chunk < test->size ? at + chunk : test->size;

      while (at < end && !failed) {
        size_t used =
            dots_reader_feed(&reader, test->stream + at, end - at, &event);

        ASSERTm(message, used > 0 || event.type != DOTS_EVENT_NONE);
        if (event.type == DOTS_EVENT_CHUNK) {
          ASSERT_EQm(message, 16 + text_size, event.offset);
          ASSERTm(message, text_size + event.chunk.size <= sizeof text);
          memcpy(text + text_size, event.chunk.octets, event.chunk.size);
          text_size += event.chunk.size;
        }
        failed = event.type == DOTS_EVENT_ERROR;
        at += used;
      }
    }
    dots_reader_finish(&reader, &event);
    ASSERT_EQm(message, DOTS_EVENT_ERROR, event.type);
    ASSERT_EQm(message, test->code, event.code);
    ASSERT_EQm(message, test->offset, event.offset);
    ASSERT_EQm(message, test->characters_size, text_size);
    ASSERT_MEM_EQm(message, test->characters, text, text_size);
  }
  PASS();
}

SUITE(reader) {
  size_t i;

  RUN_TEST(code_names);
  RUN_TEST(header);
  RUN_TEST(events);
  for (i = 0; i < sizeof text_error_cases / sizeof text_error_cases[0]; ++i) {
    greatest_set_test_suffix(text_error_cases[i].name);
    RUN_TEST1(text_error, &text_error_cases[i]);
  }
}

/* A leb128, and what its last octet does to it. */
struct leb128_case {
  const char *name;
  unsigned char octets[16];
  size_t size;
  enum dots__leb128_result result;
  uint64_t value; /* if result is DOTS__LEB128_DONE */
};

static const struct leb128_case leb128_cases[] = {
    {"zero", {0x00}, 1, DOTS__LEB128_DONE, 0},
    {"one-octet-max", {0x7F}, 1, DOTS__LEB128_DONE, 0x7F},
    {"two-octets", {0x80, 0x01}, 2, DOTS__LEB128_DONE, 0x80},
    {"three-octets", {0xE5, 0x8E, 0x26}, 3, DOTS__LEB128_DONE, 624485},
    {"zero-groups",
     {0x80, 0x80, 0x80, 0x01},
     4,
     DOTS__LEB128_DONE,
     UINT64_C(1) << 21},
    {"bit-63",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01},
     10,
     DOTS__LEB128_DONE,
     UINT64_C(1) << 63},
    {"max",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01},
     10,
     DOTS__LEB128_DONE,
     UINT64_MAX},
    {"bit-64",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02},
     10,
     DOTS__LEB128_OVERFLOW,
     0},
    {"tenth-octet-max",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F},
     10,
     DOTS__LEB128_OVERFLOW,
     0},
    {"eleventh-octet",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01},
     11,
     DOTS__LEB128_OVERFLOW,
     0},
    {"zero-tail", {0x80, 0x00}, 2, DOTS__LEB128_MALFORMED, 0},
    {"zero-tail-past-tenth",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00},
     12,
     DOTS__LEB128_MALFORMED,
     0},
    {"zero-tail-after-overflow",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x82, 0x00},
     11,
     DOTS__LEB128_MALFORMED,
     0}};

/* Pushes a leb128 at offset 1000, octet by octet. */
TEST leb128_push(const struct leb128_case *test) {
  struct dots__leb128 leb128 = DOTS__ZERO;
  size_t i;

  for (i = 0; i + 1 < test->size; ++i) {
    ASSERT_EQ(DOTS__LEB128_MORE,
              dots__leb128_push(&leb128, test->octets[i], 1000 + i));
  }
  ASSERT_EQ(test->result,
            dots__leb128_push(&leb128, test->octets[i], 1000 + i));
  ASSERT_EQ(1000, leb128.offset);
  if (test->result == DOTS__LEB128_DONE) {
    ASSERT_EQ(test->value, leb128.value);
  }
  PASS();
}

/* A leb128 after a complete one starts anew. */
TEST leb128_in_a_row(void) {
  struct dots__leb128 leb128 = DOTS__ZERO;

  ASSERT_EQ(DOTS__LEB128_MORE, dots__leb128_push(&leb128, 0x80, 0));
  ASSERT_EQ(DOTS__LEB128_DONE, dots__leb128_push(&leb128, 0x01, 1));
  ASSERT_EQ(DOTS__LEB128_DONE, dots__leb128_push(&leb128, 0x05, 2));
  ASSERT_EQ(2, leb128.offset);
  ASSERT_EQ(5, leb128.value);
  PASS();
}

SUITE(leb128) {
  size_t i;

  for (i = 0; i < sizeof leb128_cases / sizeof leb128_cases[0]; ++i) {
    greatest_set_test_suffix(leb128_cases[i].name);
    RUN_TEST1(leb128_push, &leb128_cases[i]);
  }
  RUN_TEST(leb128_in_a_row);
}

/* A UTF-8 sequence, what its last octet does to it, and its length. */
struct utf8_case {
  const char *name;
  unsigned char octets[4];
  size_t size;
  enum dots__utf8_result result;
  unsigned length; /* 0 for an invalid first octet */
};

static const struct utf8_case utf8_cases[] = {
    {"one-octet-min", {0x00}, 1, DOTS__UTF8_DONE, 1},
    {"one-octet-max", {0x7F}, 1, DOTS__UTF8_DONE, 1},
    {"two-octets-min", {0xC2, 0x80}, 2, DOTS__UTF8_DONE, 2},
    {"two-octets-max", {0xDF, 0xBF}, 2, DOTS__UTF8_DONE, 2},
    {"e0-min", {0xE0, 0xA0, 0x80}, 3, DOTS__UTF8_DONE, 3},
    {"euro", {0xE2, 0x82, 0xAC}, 3, DOTS__UTF8_DONE, 3},
    {"before-surrogates", {0xED, 0x9F, 0xBF}, 3, DOTS__UTF8_DONE, 3},
    {"after-surrogates", {0xEE, 0x80, 0x80}, 3, DOTS__UTF8_DONE, 3},
    {"three-octets-max", {0xEF, 0xBF, 0xBF}, 3, DOTS__UTF8_DONE, 3},
    {"f0-min", {0xF0, 0x90, 0x80, 0x80}, 4, DOTS__UTF8_DONE, 4},
    {"f1", {0xF1, 0x80, 0x80, 0x80}, 4, DOTS__UTF8_DONE, 4},
    {"four-octets-max", {0xF4, 0x8F, 0xBF, 0xBF}, 4, DOTS__UTF8_DONE, 4},
    {"lead-80", {0x80}, 1, DOTS__UTF8_INVALID, 0},
    {"lead-bf", {0xBF}, 1, DOTS__UTF8_INVALID, 0},
    {"lead-c0", {0xC0}, 1, DOTS__UTF8_INVALID, 0},
    {"lead-c1", {0xC1}, 1, DOTS__UTF8_INVALID, 0},
    {"lead-f5", {0xF5}, 1, DOTS__UTF8_INVALID, 0},
    {"lead-ff", {0xFF}, 1, DOTS__UTF8_INVALID, 0},
    {"second-below-80", {0xC2, 0x7F}, 2, DOTS__UTF8_INVALID, 2},
    {"second-above-bf", {0xC2, 0xC0}, 2, DOTS__UTF8_INVALID, 2},
    {"e0-overlong", {0xE0, 0x9F}, 2, DOTS__UTF8_INVALID, 3},
    {"surrogate", {0xED, 0xA0}, 2, DOTS__UTF8_INVALID, 3},
    {"f0-overlong", {0xF0, 0x8F}, 2, DOTS__UTF8_INVALID, 4},
    {"above-10ffff", {0xF4, 0x90}, 2, DOTS__UTF8_INVALID, 4},
    {"third-octet", {0xE2, 0x82, 0x41}, 3, DOTS__UTF8_INVALID, 3},
    {"fourth-octet", {0xF0, 0x90, 0x80, 0x41}, 4, DOTS__UTF8_INVALID, 4},
    {"lead-after-lead", {0xE2, 0xE2}, 2, DOTS__UTF8_INVALID, 3}};

/*
 * Pushes a UTF-8 sequence at offset 1000, octet by octet. The octets
 * before an invalid one stay in the sequence.
 */
TEST utf8_push(const struct utf8_case *test) {
  struct dots__utf8 utf8 = DOTS__ZERO;
  size_t kept = test->result == DOTS__UTF8_DONE ? test->size : test->size - 1;
  size_t i;

  for (i = 0; i + 1 < test->size; ++i) {
    ASSERT_EQ(DOTS__UTF8_MORE,
              dots__utf8_push(&utf8, test->octets[i], 1000 + i));
  }
  ASSERT_EQ(test->result, dots__utf8_push(&utf8, test->octets[i], 1000 + i));
  ASSERT_EQ(1000, utf8.offset);
  ASSERT_EQ(test->length, utf8.length);
  ASSERT_EQ(kept, utf8.size);
  ASSERT_MEM_EQ(test->octets, utf8.octets, kept);
  PASS();
}

/* A sequence after a complete one starts anew. */
TEST utf8_in_a_row(void) {
  struct dots__utf8 utf8 = DOTS__ZERO;

  ASSERT_EQ(DOTS__UTF8_MORE, dots__utf8_push(&utf8, 0xE2, 0));
  ASSERT_EQ(DOTS__UTF8_MORE, dots__utf8_push(&utf8, 0x82, 1));
  ASSERT_EQ(DOTS__UTF8_DONE, dots__utf8_push(&utf8, 0xAC, 2));
  ASSERT_EQ(DOTS__UTF8_DONE, dots__utf8_push(&utf8, 0x41, 3));
  ASSERT_EQ(3, utf8.offset);
  ASSERT_EQ(1, utf8.length);
  ASSERT_EQ(1, utf8.size);
  ASSERT_EQ(0x41, utf8.octets[0]);
  PASS();
}

SUITE(utf8) {
  size_t i;

  for (i = 0; i < sizeof utf8_cases / sizeof utf8_cases[0]; ++i) {
    greatest_set_test_suffix(utf8_cases[i].name);
    RUN_TEST1(utf8_push, &utf8_cases[i]);
  }
  RUN_TEST(utf8_in_a_row);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(reader);
  RUN_SUITE(leb128);
  RUN_SUITE(utf8);
  GREATEST_MAIN_END();
}
