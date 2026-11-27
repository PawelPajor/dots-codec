/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * The unit tests: one SUITE per unit, the reader, the writer, the
 * composer, the model and the internal sub-APIs of dots.h.
 */

#define DOTS_IMPLEMENTATION
#define DOTS_STDIO
#include "dots.h"

#include "greatest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TEST code_names(void) {
  ASSERT_STR_EQ("OK", dots_code_name(DOTS_OK));
  ASSERT_STR_EQ("INVALID_TEXT", dots_code_name(DOTS_INVALID_TEXT));
  ASSERT_EQ(NULL, dots_code_name((enum dots_code)9));
  PASS();
}

/* A header with multi-octet values, frame-rate 2^64 - 1 among them. */
TEST header(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x85, 0x01, 0x01, 0xC8, 0x01, 0x04,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0xFF};
  struct dots_evt_reader reader;
  struct dots_evt_event event;
  size_t used;

  dots_evt_reader_init(&reader);
  used = dots_evt_reader_feed(&reader, stream, sizeof stream, &event);
  ASSERT_EQ(sizeof stream - 1, used);
  ASSERT_EQ(DOTS_EVT_EVENT_HEADER, event.type);
  ASSERT_EQ(0, event.header.version.major);
  ASSERT_EQ(1, event.header.version.minor);
  ASSERT_EQ(133, event.header.version.update);
  ASSERT_EQ(1, event.header.flags);
  ASSERT_EQ(200, event.header.dimensions.x);
  ASSERT_EQ(4, event.header.dimensions.y);
  ASSERT_EQ(UINT64_MAX, event.header.frame_rate);
  PASS();
}

/* The events of two frames, fed whole, then octets after the end. */
TEST events(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04,
      0x0A, 0x01, 0x02, 0x03, 0x01, 0x02, 0x39, 0x47, 0xFE, 0xFD,
      0xFE, 0x02, 0x00, 0x00, 0x00, 0xFD, 0xFF, 0x05};
  static const enum dots_evt_event_type types[] = {
      DOTS_EVT_EVENT_HEADER,        DOTS_EVT_EVENT_FRAME,
      DOTS_EVT_EVENT_RUN,           DOTS_EVT_EVENT_CHUNK,
      DOTS_EVT_EVENT_END_OF_BLOCKS, DOTS_EVT_EVENT_END_OF_TEXT,
      DOTS_EVT_EVENT_FRAME,         DOTS_EVT_EVENT_END_OF_BLOCKS,
      DOTS_EVT_EVENT_RUN,           DOTS_EVT_EVENT_END_OF_TEXT,
      DOTS_EVT_EVENT_END_OF_STREAM, DOTS_EVT_EVENT_NONE};
  static const uint64_t offsets[] = {0,  11, 12, 16, 18, 19,
                                     20, 20, 21, 25, 26, 0};
  struct dots_evt_reader reader;
  struct dots_evt_event event;
  size_t at = 0;
  size_t i;

  dots_evt_reader_init(&reader);
  for (i = 0; i < sizeof types / sizeof types[0]; ++i) {
    at +=
        dots_evt_reader_feed(&reader, stream + at, sizeof stream - at, &event);
    ASSERT_EQ(types[i], event.type);
    ASSERT_EQ(offsets[i], event.offset);
    if (i == 1) {
      ASSERT(event.clear);
    } else if (i == 2) {
      ASSERT_FALSE(event.run.text);
      ASSERT_EQ(3, event.run.x);
      ASSERT_EQ(1, event.run.y);
      ASSERT_EQ(2, event.run.count);
    } else if (i == 3) {
      ASSERT_EQ(stream + 16, event.chunk.octets);
      ASSERT_EQ(2, event.chunk.size);
    } else if (i == 6) {
      ASSERT_FALSE(event.clear);
    } else if (i == 8) {
      ASSERT(event.run.text);
      ASSERT_EQ(0, event.run.count);
    }
  }
  ASSERT_EQ(sizeof stream, at);
  dots_evt_reader_finish(&reader, &event);
  ASSERT_EQ(DOTS_EVT_EVENT_END_OF_STREAM, event.type);
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
    struct dots_evt_reader reader;
    struct dots_evt_event event;
    unsigned char text[sizeof test->stream];
    size_t text_size = 0;
    size_t at = 0;
    int failed = 0;

    snprintf(message, sizeof message, "in chunks of %lu", (unsigned long)chunk);
    dots_evt_reader_init(&reader);
    while (at < test->size && !failed) {
      size_t end = at + chunk < test->size ? at + chunk : test->size;

      while (at < end && !failed) {
        size_t used =
            dots_evt_reader_feed(&reader, test->stream + at, end - at, &event);

        ASSERTm(message, used > 0 || event.type != DOTS_EVT_EVENT_NONE);
        if (event.type == DOTS_EVT_EVENT_CHUNK) {
          ASSERT_EQm(message, 16 + text_size, event.offset);
          ASSERTm(message, text_size + event.chunk.size <= sizeof text);
          memcpy(text + text_size, event.chunk.octets, event.chunk.size);
          text_size += event.chunk.size;
        }
        failed = event.type == DOTS_EVT_EVENT_ERROR;
        at += used;
      }
    }
    dots_evt_reader_finish(&reader, &event);
    ASSERT_EQm(message, DOTS_EVT_EVENT_ERROR, event.type);
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

/*
 * Events for the writer. Designated initializers are not C++11, so
 * each is made by a function.
 */
static struct dots_evt_event header_event(uint64_t major, uint64_t minor,
                                          uint64_t flags, uint64_t columns,
                                          uint64_t rows) {
  struct dots_evt_event event = DOTS__ZERO;

  event.type = DOTS_EVT_EVENT_HEADER;
  event.header.version.major = major;
  event.header.version.minor = minor;
  event.header.flags = flags;
  event.header.dimensions.x = columns;
  event.header.dimensions.y = rows;
  event.header.frame_rate = 10;
  return event;
}

static struct dots_evt_event frame_event(bool clear) {
  struct dots_evt_event event = DOTS__ZERO;

  event.type = DOTS_EVT_EVENT_FRAME;
  event.clear = clear;
  return event;
}

static struct dots_evt_event run_event(bool text, uint64_t x, uint64_t y,
                                       uint64_t count) {
  struct dots_evt_event event = DOTS__ZERO;

  event.type = DOTS_EVT_EVENT_RUN;
  event.run.text = text;
  event.run.x = x;
  event.run.y = y;
  event.run.count = count;
  return event;
}

static struct dots_evt_event chunk_event(const unsigned char *octets,
                                         size_t size) {
  struct dots_evt_event event = DOTS__ZERO;

  event.type = DOTS_EVT_EVENT_CHUNK;
  event.chunk.octets = octets;
  event.chunk.size = size;
  return event;
}

static struct dots_evt_event token_event(enum dots_evt_event_type type) {
  struct dots_evt_event event = DOTS__ZERO;

  event.type = type;
  return event;
}

/* Writes an event, with room for any. */
static struct dots_evt_writer_result put(struct dots_evt_writer *writer,
                                         struct dots_evt_event event) {
  unsigned char out[DOTS_EVT_WRITER_MAX];

  return dots_evt_writer_put(writer, &event, out, sizeof out);
}

/* Starts a stream of 16 x 4 cells, and a frame without a clear. */
static void start(struct dots_evt_writer *writer, uint64_t flags) {
  dots_evt_writer_init(writer);
  put(writer, header_event(0, 1, flags, 16, 4));
  put(writer, frame_event(false));
}

#define ASSERT_WRITTEN(result) ASSERT_EQ(DOTS_EVT_WRITER_OK, (result).status)

#define ASSERT_REFUSED(status_, code_, result)                                 \
  do {                                                                         \
    struct dots_evt_writer_result refused = (result);                          \
                                                                               \
    ASSERT_EQ(status_, refused.status);                                        \
    ASSERT_EQ(code_, refused.code);                                            \
    ASSERT_EQ(0, refused.size);                                                \
  } while (0)

/*
 * The stream of the reader's test events, written: a frame without a
 * clear is written as nothing.
 */
TEST writer_stream(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10,
      0x04, 0x0A, 0x01, 0x02, 0x03, 0x01, 0x02, 0x39, 0x47,
      0xFE, 0xFD, 0xFE, 0x02, 0x00, 0x00, 0x00, 0xFD, 0xFF};
  static const unsigned char blocks[] = {0x39, 0x47};
  struct dots_evt_event events[11];
  unsigned char out[sizeof stream];
  struct dots_evt_writer writer;
  size_t size = 0;
  size_t i;

  events[0] = header_event(0, 1, 1, 16, 4);
  events[1] = frame_event(true);
  events[2] = run_event(false, 3, 1, 2);
  events[3] = chunk_event(blocks, sizeof blocks);
  events[4] = token_event(DOTS_EVT_EVENT_END_OF_BLOCKS);
  events[5] = token_event(DOTS_EVT_EVENT_END_OF_TEXT);
  events[6] = frame_event(false);
  events[7] = token_event(DOTS_EVT_EVENT_END_OF_BLOCKS);
  events[8] = run_event(true, 0, 0, 0);
  events[9] = token_event(DOTS_EVT_EVENT_END_OF_TEXT);
  events[10] = token_event(DOTS_EVT_EVENT_END_OF_STREAM);
  dots_evt_writer_init(&writer);
  for (i = 0; i < sizeof events / sizeof events[0]; ++i) {
    struct dots_evt_writer_result result =
        dots_evt_writer_put(&writer, &events[i], out + size, sizeof out - size);

    ASSERT_WRITTEN(result);
    size += result.size;
  }
  ASSERT_EQ(sizeof stream, size);
  ASSERT_MEM_EQ(stream, out, size);
  PASS();
}

/* A header of the most octets: DOTS_EVT_WRITER_MAX. */
TEST writer_max(void) {
  struct dots_evt_event header = header_event(0, 1, 0, 1, 1);
  unsigned char out[DOTS_EVT_WRITER_MAX];
  struct dots_evt_writer writer;
  struct dots_evt_writer_result result;

  header.header.version.update = UINT64_MAX;
  header.header.frame_rate = UINT64_MAX;
  dots_evt_writer_init(&writer);
  result = dots_evt_writer_put(&writer, &header, out, sizeof out);
  ASSERT_WRITTEN(result);
  ASSERT_EQ(4 + 1 + 1 + 10 + 1 + 1 + 1 + 10, result.size);
  PASS();
}

/* A character split across chunks. */
TEST writer_split_text(void) {
  static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
  struct dots_evt_writer writer;
  size_t i;

  start(&writer, 1);
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_WRITTEN(put(&writer, run_event(true, 15, 3, sizeof euro)));
  for (i = 0; i < sizeof euro; ++i) {
    ASSERT_WRITTEN(put(&writer, chunk_event(euro + i, 1)));
  }
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_TEXT)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_STREAM)));
  PASS();
}

/* Events the stream has no place for next. */
TEST writer_out_of_order(void) {
  static const unsigned char block = 0;
  struct dots_evt_writer writer;

  dots_evt_writer_init(&writer);
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, frame_event(true)));
  start(&writer, 1);
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, header_event(0, 1, 1, 16, 4)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, frame_event(true)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, token_event(DOTS_EVT_EVENT_END_OF_STREAM)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, run_event(true, 0, 0, 1)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, chunk_event(&block, 1)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, token_event(DOTS_EVT_EVENT_END_OF_TEXT)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, token_event(DOTS_EVT_EVENT_NONE)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, token_event(DOTS_EVT_EVENT_ERROR)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, run_event(false, 0, 0, 1)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_TEXT)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_STREAM)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_OUT_OF_ORDER, DOTS_OK,
                 put(&writer, frame_event(true)));
  PASS();
}

/* A chunk past the count of its run, then one that fits. */
TEST writer_past_count(void) {
  static const unsigned char blocks[] = {1, 2, 3};
  struct dots_evt_writer writer;

  start(&writer, 0);
  ASSERT_WRITTEN(put(&writer, run_event(false, 0, 0, 2)));
  ASSERT_WRITTEN(put(&writer, chunk_event(blocks, 1)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_PAST_COUNT, DOTS_OK,
                 put(&writer, chunk_event(blocks, 2)));
  ASSERT_WRITTEN(put(&writer, chunk_event(blocks, 1)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  PASS();
}

/* Octets that do not fit, then the same event with room. */
TEST writer_no_space(void) {
  static const unsigned char blocks[] = {1, 2};
  struct dots_evt_event header = header_event(0, 1, 0, 16, 4);
  struct dots_evt_event chunk = chunk_event(blocks, sizeof blocks);
  unsigned char out[DOTS_EVT_WRITER_MAX];
  struct dots_evt_writer writer;

  dots_evt_writer_init(&writer);
  ASSERT_REFUSED(DOTS_EVT_WRITER_NO_SPACE, DOTS_OK,
                 dots_evt_writer_put(&writer, &header, out, 10));
  ASSERT_WRITTEN(dots_evt_writer_put(&writer, &header, out, 11));
  ASSERT_WRITTEN(put(&writer, frame_event(false)));
  ASSERT_WRITTEN(put(&writer, run_event(false, 0, 0, sizeof blocks)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_NO_SPACE, DOTS_OK,
                 dots_evt_writer_put(&writer, &chunk, out, 1));
  ASSERT_WRITTEN(dots_evt_writer_put(&writer, &chunk, out, 2));
  PASS();
}

/* Headers that break HDR-1, HDR-3 and HDR-4. */
TEST writer_ill_formed_header(void) {
  struct dots_evt_writer writer;

  dots_evt_writer_init(&writer);
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_UNSUPPORTED_VERSION,
                 put(&writer, header_event(1, 1, 0, 16, 4)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_UNSUPPORTED_VERSION,
                 put(&writer, header_event(0, 2, 0, 16, 4)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_VALUE,
                 put(&writer, header_event(0, 1, 2, 16, 4)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_VALUE,
                 put(&writer, header_event(0, 1, 0, 0, 4)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_VALUE,
                 put(&writer, header_event(0, 1, 0, 16, 0)));
  ASSERT_WRITTEN(put(&writer, header_event(0, 1, 0, 16, 4)));
  PASS();
}

/* Runs that break HDR-5, RUN-2, DEC-3 and DEC-5. */
TEST writer_ill_formed_run(void) {
  static const unsigned char blocks[] = {1, 2};
  static const unsigned char invalid[] = {0x41, 0xC0};
  static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
  struct dots_evt_writer writer;

  start(&writer, 0);
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, run_event(false, 16, 0, 1)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, run_event(false, 0, 4, 1)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, run_event(false, 15, 0, 2)));
  ASSERT_WRITTEN(put(&writer, run_event(false, 14, 0, sizeof blocks)));
  ASSERT_WRITTEN(put(&writer, chunk_event(blocks, sizeof blocks)));

  start(&writer, 0);
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_UNEXPECTED_TEXT,
                 put(&writer, run_event(true, 0, 0, 0)));

  start(&writer, 1);
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_WRITTEN(put(&writer, run_event(true, 0, 0, sizeof invalid)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_TEXT,
                 put(&writer, chunk_event(invalid, sizeof invalid)));
  ASSERT_WRITTEN(put(&writer, chunk_event(invalid, 1)));

  start(&writer, 1);
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_WRITTEN(put(&writer, run_event(true, 0, 0, 2)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_TEXT,
                 put(&writer, chunk_event(euro, 2)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_INVALID_TEXT,
                 put(&writer, chunk_event(euro, 1)));
  ASSERT_WRITTEN(put(&writer, chunk_event(blocks, 1)));
  PASS();
}

/*
 * Runs and chunks after which no data can end the run well-formed, at
 * column 14 of 16: 2 blocks, or 8 octets of text, fit what is left.
 */
TEST writer_dead_end(void) {
  static const unsigned char text[] = {0x41, 0x41, 0xE2, 0x82, 0xAC};
  static const unsigned char emoji[] = {0xF0, 0x9F, 0x98, 0x80};
  struct dots_evt_writer writer;

  start(&writer, 1);
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, run_event(false, 14, 0, 3)));
  ASSERT_WRITTEN(put(&writer, run_event(false, 14, 0, 2)));
  ASSERT_WRITTEN(put(&writer, chunk_event(text, 1)));
  ASSERT_WRITTEN(put(&writer, chunk_event(text, 1)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_BLOCKS)));
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, run_event(true, 14, 0, 9)));
  ASSERT_WRITTEN(put(&writer, run_event(true, 14, 0, 5)));
  /* "A" leaves 4 octets for 1 cell; "AA" would leave 3 for none */
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, chunk_event(text, 2)));
  ASSERT_WRITTEN(put(&writer, chunk_event(text, 1)));
  /* a 3-octet character in the last cell would leave 1 octet over */
  ASSERT_REFUSED(DOTS_EVT_WRITER_ILL_FORMED, DOTS_OUTSIDE_GRID,
                 put(&writer, chunk_event(text + 2, 1)));
  ASSERT_WRITTEN(put(&writer, chunk_event(emoji, 2)));
  ASSERT_WRITTEN(put(&writer, chunk_event(emoji + 2, 2)));
  ASSERT_WRITTEN(put(&writer, token_event(DOTS_EVT_EVENT_END_OF_TEXT)));
  PASS();
}

/* Events with their chunks merged: the octets of all in a row. */
struct trace {
  struct dots_evt_event events[16];
  size_t count;
  unsigned char data[64];
  size_t size;
};

/* Adds an event other than NONE; returns 0 if the trace is full. */
static int trace_add(struct trace *trace, const struct dots_evt_event *event) {
  size_t i;

  if (event->type == DOTS_EVT_EVENT_NONE) {
    return 1;
  }
  if (event->type == DOTS_EVT_EVENT_CHUNK) {
    if (trace->size + event->chunk.size > sizeof trace->data) {
      return 0;
    }
    for (i = 0; i < event->chunk.size; ++i) {
      trace->data[trace->size + i] = event->chunk.octets[i];
    }
    trace->size += event->chunk.size;
    if (trace->count > 0 &&
        trace->events[trace->count - 1].type == DOTS_EVT_EVENT_CHUNK) {
      return 1;
    }
  }
  if (trace->count == sizeof trace->events / sizeof trace->events[0]) {
    return 0;
  }
  trace->events[trace->count] = *event;
  ++trace->count;
  return 1;
}

/* Whether two events of traces say the same, apart from offset. */
static int same_event(const struct dots_evt_event *a,
                      const struct dots_evt_event *b) {
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
  case DOTS_EVT_EVENT_HEADER:
    return a->header.version.major == b->header.version.major &&
           a->header.version.minor == b->header.version.minor &&
           a->header.version.update == b->header.version.update &&
           a->header.flags == b->header.flags &&
           a->header.dimensions.x == b->header.dimensions.x &&
           a->header.dimensions.y == b->header.dimensions.y &&
           a->header.frame_rate == b->header.frame_rate;
  case DOTS_EVT_EVENT_FRAME:
    return a->clear == b->clear;
  case DOTS_EVT_EVENT_RUN:
    return a->run.text == b->run.text && a->run.x == b->run.x &&
           a->run.y == b->run.y && a->run.count == b->run.count;
  case DOTS_EVT_EVENT_ERROR:
    return a->code == b->code;
  case DOTS_EVT_EVENT_NONE:
  case DOTS_EVT_EVENT_CHUNK:
  case DOTS_EVT_EVENT_END_OF_BLOCKS:
  case DOTS_EVT_EVENT_END_OF_TEXT:
  case DOTS_EVT_EVENT_END_OF_STREAM:
    return 1;
  }
  return 0;
}

/*
 * Writes a full stream from events, then reads it with a new reader in
 * chunks of every size: the same events, and the same run data, come
 * back. The data is written in chunks that cut UTF-8 sequences, and
 * read back in chunks of whole ones.
 */
TEST writer_then_reader(void) {
  static const unsigned char blocks[] = {0x01, 0x80, 0xFF, 0x00};
  static const unsigned char text[] = {0x41, 0xE2, 0x82, 0xAC,
                                       0xF0, 0x9F, 0x98, 0x80};
  struct dots_evt_event events[16];
  struct trace written = DOTS__ZERO;
  unsigned char stream[128];
  struct dots_evt_writer writer;
  size_t count = 0;
  size_t size = 0;
  size_t chunk;
  size_t i;

  events[count++] = header_event(0, 1, 1, 4, 2);
  events[count++] = frame_event(false);
  events[count++] = run_event(false, 0, 0, sizeof blocks);
  events[count++] = chunk_event(blocks, 2);
  events[count++] = chunk_event(blocks + 2, 2);
  events[count++] = token_event(DOTS_EVT_EVENT_END_OF_BLOCKS);
  events[count++] = run_event(true, 1, 1, sizeof text);
  events[count++] = chunk_event(text, 2);
  events[count++] = chunk_event(text + 2, 1);
  events[count++] = chunk_event(text + 3, 3);
  events[count++] = chunk_event(text + 6, 2);
  events[count++] = token_event(DOTS_EVT_EVENT_END_OF_TEXT);
  events[count++] = frame_event(true);
  events[count++] = token_event(DOTS_EVT_EVENT_END_OF_BLOCKS);
  events[count++] = token_event(DOTS_EVT_EVENT_END_OF_TEXT);
  events[count++] = token_event(DOTS_EVT_EVENT_END_OF_STREAM);

  dots_evt_writer_init(&writer);
  for (i = 0; i < count; ++i) {
    struct dots_evt_writer_result result = dots_evt_writer_put(
        &writer, &events[i], stream + size, sizeof stream - size);

    ASSERT_WRITTEN(result);
    size += result.size;
    ASSERT(trace_add(&written, &events[i]));
  }

  for (chunk = 1; chunk <= size; ++chunk) {
    struct trace read = DOTS__ZERO;
    struct dots_evt_reader reader;
    struct dots_evt_event event;
    size_t at = 0;

    dots_evt_reader_init(&reader);
    while (at < size) {
      size_t end = at + chunk < size ? at + chunk : size;

      while (at < end) {
        at += dots_evt_reader_feed(&reader, stream + at, end - at, &event);
        ASSERT(event.type != DOTS_EVT_EVENT_ERROR);
        ASSERT(trace_add(&read, &event));
      }
    }
    dots_evt_reader_finish(&reader, &event);
    ASSERT_EQ(DOTS_EVT_EVENT_END_OF_STREAM, event.type);
    ASSERT_EQ(written.count, read.count);
    for (i = 0; i < written.count; ++i) {
      ASSERT(same_event(&written.events[i], &read.events[i]));
    }
    ASSERT_EQ(written.size, read.size);
    ASSERT_MEM_EQ(written.data, read.data, written.size);
  }
  PASS();
}

SUITE(writer) {
  RUN_TEST(writer_stream);
  RUN_TEST(writer_max);
  RUN_TEST(writer_split_text);
  RUN_TEST(writer_out_of_order);
  RUN_TEST(writer_past_count);
  RUN_TEST(writer_no_space);
  RUN_TEST(writer_ill_formed_header);
  RUN_TEST(writer_ill_formed_run);
  RUN_TEST(writer_dead_end);
  RUN_TEST(writer_then_reader);
}

/* A leb128, and what its last octet does to it. */
struct leb128_case {
  const char *name;
  unsigned char octets[16];
  size_t size;
  enum dots__leb128_reader_result result;
  uint64_t value; /* if result is DOTS__LEB128_READER_DONE */
};

static const struct leb128_case leb128_cases[] = {
    {"zero", {0x00}, 1, DOTS__LEB128_READER_DONE, 0},
    {"one-octet-max", {0x7F}, 1, DOTS__LEB128_READER_DONE, 0x7F},
    {"two-octets", {0x80, 0x01}, 2, DOTS__LEB128_READER_DONE, 0x80},
    {"three-octets", {0xE5, 0x8E, 0x26}, 3, DOTS__LEB128_READER_DONE, 624485},
    {"zero-groups",
     {0x80, 0x80, 0x80, 0x01},
     4,
     DOTS__LEB128_READER_DONE,
     UINT64_C(1) << 21},
    {"bit-63",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01},
     10,
     DOTS__LEB128_READER_DONE,
     UINT64_C(1) << 63},
    {"max",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01},
     10,
     DOTS__LEB128_READER_DONE,
     UINT64_MAX},
    {"bit-64",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02},
     10,
     DOTS__LEB128_READER_OVERFLOW,
     0},
    {"tenth-octet-max",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F},
     10,
     DOTS__LEB128_READER_OVERFLOW,
     0},
    {"eleventh-octet",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01},
     11,
     DOTS__LEB128_READER_OVERFLOW,
     0},
    {"zero-tail", {0x80, 0x00}, 2, DOTS__LEB128_READER_MALFORMED, 0},
    {"zero-tail-past-tenth",
     {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00},
     12,
     DOTS__LEB128_READER_MALFORMED,
     0},
    {"zero-tail-after-overflow",
     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x82, 0x00},
     11,
     DOTS__LEB128_READER_MALFORMED,
     0}};

/* Reads a leb128 at offset 1000, octet by octet. */
TEST leb128_read(const struct leb128_case *test) {
  struct dots__leb128_reader leb128 = DOTS__ZERO;
  size_t i;

  for (i = 0; i + 1 < test->size; ++i) {
    ASSERT_EQ(DOTS__LEB128_READER_MORE,
              dots__leb128_read(&leb128, test->octets[i], 1000 + i));
  }
  ASSERT_EQ(test->result,
            dots__leb128_read(&leb128, test->octets[i], 1000 + i));
  ASSERT_EQ(1000, leb128.offset);
  if (test->result == DOTS__LEB128_READER_DONE) {
    ASSERT_EQ(test->value, leb128.value);
  }
  PASS();
}

/* Writes the value of a well-formed leb128: the same octets. */
TEST leb128_write(const struct leb128_case *test) {
  unsigned char octets[10];

  ASSERT_EQ(test->size, dots__leb128_write(test->value, octets));
  ASSERT_MEM_EQ(test->octets, octets, test->size);
  PASS();
}

/* A leb128 after a complete one starts anew. */
TEST leb128_in_a_row(void) {
  struct dots__leb128_reader leb128 = DOTS__ZERO;

  ASSERT_EQ(DOTS__LEB128_READER_MORE, dots__leb128_read(&leb128, 0x80, 0));
  ASSERT_EQ(DOTS__LEB128_READER_DONE, dots__leb128_read(&leb128, 0x01, 1));
  ASSERT_EQ(DOTS__LEB128_READER_DONE, dots__leb128_read(&leb128, 0x05, 2));
  ASSERT_EQ(2, leb128.offset);
  ASSERT_EQ(5, leb128.value);
  PASS();
}

SUITE(leb128) {
  size_t i;

  for (i = 0; i < sizeof leb128_cases / sizeof leb128_cases[0]; ++i) {
    greatest_set_test_suffix(leb128_cases[i].name);
    RUN_TEST1(leb128_read, &leb128_cases[i]);
  }
  RUN_TEST(leb128_in_a_row);
  for (i = 0; i < sizeof leb128_cases / sizeof leb128_cases[0]; ++i) {
    if (leb128_cases[i].result == DOTS__LEB128_READER_DONE) {
      greatest_set_test_suffix(leb128_cases[i].name);
      RUN_TEST1(leb128_write, &leb128_cases[i]);
    }
  }
}

/*
 * A UTF-8 sequence, what its last octet does to it, its length and, if
 * it is well-formed, its character.
 */
struct utf8_case {
  const char *name;
  unsigned char octets[4];
  size_t size;
  enum dots__utf8_result result;
  unsigned length; /* 0 for an invalid first octet */
  uint32_t character;
};

static const struct utf8_case utf8_cases[] = {
    {"one-octet-min", {0x00}, 1, DOTS__UTF8_DONE, 1, 0},
    {"one-octet-max", {0x7F}, 1, DOTS__UTF8_DONE, 1, 0x7F},
    {"two-octets-min", {0xC2, 0x80}, 2, DOTS__UTF8_DONE, 2, 0x80},
    {"two-octets-max", {0xDF, 0xBF}, 2, DOTS__UTF8_DONE, 2, 0x7FF},
    {"e0-min", {0xE0, 0xA0, 0x80}, 3, DOTS__UTF8_DONE, 3, 0x800},
    {"euro", {0xE2, 0x82, 0xAC}, 3, DOTS__UTF8_DONE, 3, 0x20AC},
    {"before-surrogates", {0xED, 0x9F, 0xBF}, 3, DOTS__UTF8_DONE, 3, 0xD7FF},
    {"after-surrogates", {0xEE, 0x80, 0x80}, 3, DOTS__UTF8_DONE, 3, 0xE000},
    {"three-octets-max", {0xEF, 0xBF, 0xBF}, 3, DOTS__UTF8_DONE, 3, 0xFFFF},
    {"f0-min", {0xF0, 0x90, 0x80, 0x80}, 4, DOTS__UTF8_DONE, 4, 0x10000},
    {"f1", {0xF1, 0x80, 0x80, 0x80}, 4, DOTS__UTF8_DONE, 4, 0x40000},
    {"four-octets-max",
     {0xF4, 0x8F, 0xBF, 0xBF},
     4,
     DOTS__UTF8_DONE,
     4,
     0x10FFFF},
    {"lead-80", {0x80}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"lead-bf", {0xBF}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"lead-c0", {0xC0}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"lead-c1", {0xC1}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"lead-f5", {0xF5}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"lead-ff", {0xFF}, 1, DOTS__UTF8_INVALID, 0, 0},
    {"second-below-80", {0xC2, 0x7F}, 2, DOTS__UTF8_INVALID, 2, 0},
    {"second-above-bf", {0xC2, 0xC0}, 2, DOTS__UTF8_INVALID, 2, 0},
    {"e0-overlong", {0xE0, 0x9F}, 2, DOTS__UTF8_INVALID, 3, 0},
    {"surrogate", {0xED, 0xA0}, 2, DOTS__UTF8_INVALID, 3, 0},
    {"f0-overlong", {0xF0, 0x8F}, 2, DOTS__UTF8_INVALID, 4, 0},
    {"above-10ffff", {0xF4, 0x90}, 2, DOTS__UTF8_INVALID, 4, 0},
    {"third-octet", {0xE2, 0x82, 0x41}, 3, DOTS__UTF8_INVALID, 3, 0},
    {"fourth-octet", {0xF0, 0x90, 0x80, 0x41}, 4, DOTS__UTF8_INVALID, 4, 0},
    {"lead-after-lead", {0xE2, 0xE2}, 2, DOTS__UTF8_INVALID, 3, 0}};

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
  if (test->result == DOTS__UTF8_DONE) {
    ASSERT_EQ(test->character, dots__utf8_decode(utf8.octets, utf8.length));
  }
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

/*
 * Reads a stream, in chunks of the given size, into a composer of
 * count cells. Keeps the cell at index watched after each frame, up to
 * 4 frames. Returns false if the reader gave an error or the composer
 * refused the cells.
 */
static int compose(const unsigned char *stream, size_t size, size_t chunk,
                   struct dots_cell *cells, size_t count, size_t watched,
                   struct dots_cell after[4], size_t *frames) {
  struct dots_evt_reader reader;
  struct dots_evt_composer composer;
  size_t at = 0;

  *frames = 0;
  dots_evt_reader_init(&reader);
  while (at < size) {
    size_t end = at + chunk < size ? at + chunk : size;

    while (at < end) {
      struct dots_evt_event event;
      size_t used =
          dots_evt_reader_feed(&reader, stream + at, end - at, &event);

      if (event.type == DOTS_EVT_EVENT_ERROR) {
        return 0;
      }
      if (event.type == DOTS_EVT_EVENT_HEADER) {
        if (!dots_evt_composer_init(&composer, &event.header, cells, count)) {
          return 0;
        }
      } else {
        dots_evt_composer_apply(&composer, &event);
      }
      if (event.type == DOTS_EVT_EVENT_END_OF_TEXT && *frames < 4) {
        after[*frames] = cells[watched];
        ++*frames;
      }
      at += used;
    }
  }
  return 1;
}

/*
 * The example of Section 6 of dots-content.md, at cell (3, 0): a block,
 * then a character, then the eraser, then a clear.
 */
TEST composer_example(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04, 0x0A, 0x02,
      0x03, 0x00, 0x01, 0xFF, 0xFE, 0x02, 0x03, 0x00, 0x01, 0x41, 0xFD, 0xFE,
      0x02, 0x03, 0x00, 0x01, 0x00, 0xFD, 0x01, 0xFE, 0xFD, 0xFF};
  struct dots_cell cells[64];
  struct dots_cell after[4];
  size_t frames;
  size_t chunk;

  for (chunk = 1; chunk <= sizeof stream; ++chunk) {
    ASSERT(compose(stream, sizeof stream, chunk, cells, 64, 3, after, &frames));
    ASSERT_EQ(3, frames);
    ASSERT_EQ(0xFF, after[0].block);
    ASSERT_EQ(0x41, after[0].character);
    ASSERT_EQ(0xFF, after[1].block);
    ASSERT_EQ(0, after[1].character);
    ASSERT_EQ(0x00, after[2].block);
    ASSERT_EQ(0, after[2].character);
  }
  PASS();
}

/* Characters of 2, 3 and 4 octets, at (0, 1) to (3, 1). */
TEST composer_characters(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04, 0x0A,
      0xFE, 0x02, 0x00, 0x01, 0x0D, 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0,
      0x9F, 0x98, 0x80, 0xF4, 0x8F, 0xBF, 0xBF, 0xFD, 0xFF};
  static const uint32_t characters[] = {0xE9, 0x20AC, 0x1F600, 0x10FFFF, 0};
  struct dots_cell cells[64];
  struct dots_cell after[4];
  size_t frames;
  size_t chunk;
  size_t i;

  for (chunk = 1; chunk <= sizeof stream; ++chunk) {
    ASSERT(compose(stream, sizeof stream, chunk, cells, 64, 0, after, &frames));
    for (i = 0; i < sizeof characters / sizeof characters[0]; ++i) {
      ASSERT_EQ(characters[i], cells[16 + i].character);
    }
  }
  PASS();
}

/* Too few cells are refused, and left as they are. */
TEST composer_count(void) {
  struct dots_header header = DOTS__ZERO;
  struct dots_evt_composer composer;
  struct dots_cell cells[64];
  size_t i;

  for (i = 0; i < 64; ++i) {
    cells[i].block = 0x55;
    cells[i].character = 0x41;
  }
  header.dimensions.x = 16;
  header.dimensions.y = 4;
  ASSERT_FALSE(dots_evt_composer_init(&composer, &header, cells, 63));
  header.dimensions.x = UINT64_MAX;
  header.dimensions.y = 2;
  ASSERT_FALSE(dots_evt_composer_init(&composer, &header, cells, SIZE_MAX));
  header.dimensions.x = UINT64_C(1) << 32;
  header.dimensions.y = UINT64_C(1) << 32;
  ASSERT_FALSE(dots_evt_composer_init(&composer, &header, cells, SIZE_MAX));
  for (i = 0; i < 64; ++i) {
    ASSERT_EQ(0x55, cells[i].block);
    ASSERT_EQ(0x41, cells[i].character);
  }
  header.dimensions.x = 16;
  header.dimensions.y = 4;
  ASSERT(dots_evt_composer_init(&composer, &header, cells, 64));
  ASSERT_EQ(0x00, cells[63].block);
  ASSERT_EQ(0, cells[63].character);
  PASS();
}

/* Section 5 of dots-content.md: each bit, and its example 0x39. */
TEST braille(void) {
  static const uint32_t bits[8] = {0x2801, 0x2808, 0x2802, 0x2810,
                                   0x2804, 0x2820, 0x2840, 0x2880};
  unsigned n;

  ASSERT_EQ(0x2800, dots_braille(0x00));
  for (n = 0; n < 8; ++n) {
    ASSERT_EQ(bits[n], dots_braille((unsigned char)(1u << n)));
  }
  ASSERT_EQ(0x2835, dots_braille(0x39));
  ASSERT_EQ(0x28FF, dots_braille(0xFF));
  PASS();
}

SUITE(composer) {
  RUN_TEST(composer_example);
  RUN_TEST(composer_characters);
  RUN_TEST(composer_count);
  RUN_TEST(braille);
}

/* The model: an input and an output in memory. */
struct memory {
  unsigned char octets[256];
  size_t size;
  size_t next;    /* the next octet got */
  size_t fail_at; /* get or put fails at this octet */
};

static int memory_get(void *handle) {
  struct memory *memory = (struct memory *)handle;

  if (memory->next == memory->fail_at) {
    return DOTS_MDL_IO_ERROR;
  }
  if (memory->next == memory->size) {
    return DOTS_MDL_IO_END;
  }
  return memory->octets[memory->next++];
}

static int memory_put(int octet, void *handle) {
  struct memory *memory = (struct memory *)handle;

  if (memory->size == memory->fail_at ||
      memory->size == sizeof memory->octets) {
    return DOTS_MDL_IO_ERROR;
  }
  memory->octets[memory->size++] = (unsigned char)octet;
  return 0;
}

static struct memory memory_of(const unsigned char *octets, size_t size) {
  struct memory memory;

  if (size > 0) {
    memcpy(memory.octets, octets, size);
  }
  memory.size = size;
  memory.next = 0;
  memory.fail_at = SIZE_MAX;
  return memory;
}

/* An allocator that counts, and fails from the nth allocation. */
static size_t blocks_held;
static size_t allocations;
static size_t fail_from;

static void *counting_allocate(size_t size) {
  void *pointer;

  if (allocations >= fail_from) {
    return NULL;
  }
  pointer = malloc(size);
  if (pointer != NULL) {
    ++allocations;
    ++blocks_held;
  }
  return pointer;
}

static void counting_free(void *pointer) {
  if (pointer != NULL) {
    --blocks_held;
  }
  free(pointer);
}

static const struct dots_mdl_allocator counting = {counting_allocate,
                                                   counting_free};

static void counting_reset(size_t fail) {
  blocks_held = 0;
  allocations = 0;
  fail_from = fail;
}

/* The example of the main page: "Hi" at the top left of 16 x 4. */
static const unsigned char hi_stream[] = {
    0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04, 0x0A,
    0x01, 0xFE, 0x02, 0x00, 0x00, 0x02, 0x48, 0x69, 0xFD, 0xFF};

TEST model_read(void) {
  struct memory memory = memory_of(hi_stream, sizeof hi_stream);
  struct dots_mdl_input input = {memory_get, &memory};
  struct dots_mdl_source source;
  struct dots_header header;
  struct dots_mdl_frame *frame;
  struct dots_mdl_result result;
  struct dots_mdl_run *run;

  counting_reset(SIZE_MAX);
  result = dots_mdl_open_source(&source, counting, input, &header);
  ASSERT_EQ(DOTS_MDL_OK, result.status);
  ASSERT_EQ(1, header.flags);
  ASSERT_EQ(16, header.dimensions.x);
  ASSERT_EQ(4, header.dimensions.y);
  ASSERT_EQ(10, header.frame_rate);
  result = dots_mdl_get_frame(&source, &frame);
  ASSERT_EQ(DOTS_MDL_OK, result.status);
  ASSERT(frame->clear);
  ASSERT_EQ(NULL, frame->blocks.runs);
  run = frame->text.runs;
  ASSERT(run != NULL);
  ASSERT_EQ(0, run->position.x);
  ASSERT_EQ(0, run->position.y);
  ASSERT_EQ(2, run->count);
  ASSERT_MEM_EQ("Hi", run->data, 2);
  ASSERT_EQ(NULL, run->next);
  dots_mdl_release_frame(&source, frame);
  ASSERT_EQ(0, blocks_held);
  ASSERT_EQ(DOTS_MDL_END, dots_mdl_get_frame(&source, &frame).status);
  ASSERT_EQ(NULL, frame);
  ASSERT_EQ(DOTS_MDL_END, dots_mdl_get_frame(&source, &frame).status);
  PASS();
}

static struct dots_header hi_header(void) {
  struct dots_header header;

  memset(&header, 0, sizeof header);
  header.version.major = DOTS_SPEC_MAJOR;
  header.version.minor = DOTS_SPEC_MINOR;
  header.flags = 1;
  header.dimensions.x = 16;
  header.dimensions.y = 4;
  header.frame_rate = 10;
  return header;
}

TEST model_write(void) {
  struct memory memory = memory_of(NULL, 0);
  struct dots_mdl_output output = {memory_put, &memory};
  struct dots_header header = hi_header();
  struct dots_mdl_sink sink;
  unsigned char text[] = "Hi";
  struct dots_mdl_run hi = {{0, 0}, 2, text, NULL};
  struct dots_mdl_frame frame = {true, {NULL}, {&hi}};

  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_open_sink(&sink, output, &header).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_put_frame(&sink, &frame).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_close_sink(&sink).status);
  ASSERT_EQ(sizeof hi_stream, memory.size);
  ASSERT_MEM_EQ(hi_stream, memory.octets, sizeof hi_stream);
  ASSERT_EQ(DOTS_MDL_OUT_OF_ORDER, dots_mdl_put_frame(&sink, &frame).status);
  ASSERT_EQ(DOTS_MDL_OUT_OF_ORDER, dots_mdl_close_sink(&sink).status);
  ASSERT_EQ(sizeof hi_stream, memory.size);
  PASS();
}

/* Two frames, the first with runs longer than a chunk of the sink. */
TEST model_round_trip(void) {
  struct memory memory = memory_of(NULL, 0);
  struct dots_mdl_output output = {memory_put, &memory};
  struct dots_mdl_input input = {memory_get, &memory};
  struct dots_header header = hi_header();
  struct dots_header read;
  struct dots_mdl_sink sink;
  struct dots_mdl_source source;
  unsigned char blocks[100];
  unsigned char text[] = "\xE2\xA0\x81z";
  struct dots_mdl_run second = {{1, 1}, 4, text, NULL};
  struct dots_mdl_run first = {{0, 1}, 0, NULL, &second};
  struct dots_mdl_run row = {{0, 0}, 100, blocks, NULL};
  struct dots_mdl_frame frames[2] = {{true, {&row}, {&first}},
                                     {false, {NULL}, {NULL}}};
  struct dots_mdl_frame *frame;
  size_t i;

  header.dimensions.x = 100;
  for (i = 0; i < sizeof blocks; ++i) {
    blocks[i] = (unsigned char)i;
  }
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_open_sink(&sink, output, &header).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_put_frame(&sink, &frames[0]).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_put_frame(&sink, &frames[1]).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_close_sink(&sink).status);
  counting_reset(SIZE_MAX);
  ASSERT_EQ(DOTS_MDL_OK,
            dots_mdl_open_source(&source, counting, input, &read).status);
  ASSERT_EQ(100, read.dimensions.x);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_get_frame(&source, &frame).status);
  ASSERT(frame->clear);
  ASSERT_EQ(100, frame->blocks.runs->count);
  ASSERT_MEM_EQ(blocks, frame->blocks.runs->data, sizeof blocks);
  ASSERT_EQ(NULL, frame->blocks.runs->next);
  ASSERT_EQ(0, frame->text.runs->count);
  ASSERT_EQ(NULL, frame->text.runs->data);
  ASSERT_EQ(1, frame->text.runs->next->position.x);
  ASSERT_MEM_EQ(text, frame->text.runs->next->data, 4);
  ASSERT_EQ(NULL, frame->text.runs->next->next);
  dots_mdl_release_frame(&source, frame);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_get_frame(&source, &frame).status);
  ASSERT_FALSE(frame->clear);
  ASSERT_EQ(NULL, frame->blocks.runs);
  ASSERT_EQ(NULL, frame->text.runs);
  dots_mdl_release_frame(&source, frame);
  ASSERT_EQ(DOTS_MDL_END, dots_mdl_get_frame(&source, &frame).status);
  ASSERT_EQ(0, blocks_held);
  PASS();
}

/* The reader's code and offset, final, and nothing left allocated. */
TEST model_ill_formed(void) {
  struct memory memory = memory_of(hi_stream, sizeof hi_stream - 3);
  struct dots_mdl_input input = {memory_get, &memory};
  struct dots_mdl_source source;
  struct dots_header header;
  struct dots_mdl_frame *frame;
  struct dots_mdl_result result;

  counting_reset(SIZE_MAX);
  ASSERT_EQ(DOTS_MDL_OK,
            dots_mdl_open_source(&source, counting, input, &header).status);
  result = dots_mdl_get_frame(&source, &frame);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_TRUNCATED, result.code);
  ASSERT_EQ(sizeof hi_stream - 3, result.offset);
  ASSERT_EQ(NULL, frame);
  ASSERT_EQ(0, blocks_held);
  result = dots_mdl_get_frame(&source, &frame);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_TRUNCATED, result.code);
  memory = memory_of(hi_stream, 3);
  result = dots_mdl_open_source(&source, counting, input, &header);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_TRUNCATED, result.code);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, dots_mdl_get_frame(&source, &frame).status);
  PASS();
}

/*
 * A run past its row is allocated from its count, and the reader
 * reports the first block or character outside the grid (DEC-5).
 */
TEST model_run_past_row(void) {
  static const unsigned char blocks[] = {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01,
                                         0x00, 0x01, 0x10, 0x04, 0x0A, 0x01,
                                         0x02, 0x0F, 0x00, 0x03, 0x01, 0x02};
  static const unsigned char text[] = {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00,
                                       0x01, 0x10, 0x04, 0x0A, 0x01, 0xFE, 0x02,
                                       0x0F, 0x00, 0x05, 0x41, 0x42};
  struct memory memory = memory_of(blocks, sizeof blocks);
  struct dots_mdl_input input = {memory_get, &memory};
  struct dots_mdl_source source;
  struct dots_header header;
  struct dots_mdl_frame *frame;
  struct dots_mdl_result result;

  counting_reset(SIZE_MAX);
  dots_mdl_open_source(&source, counting, input, &header);
  result = dots_mdl_get_frame(&source, &frame);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_OUTSIDE_GRID, result.code);
  ASSERT_EQ(sizeof blocks - 1, result.offset);
  ASSERT_EQ(3, allocations); /* the frame, the run and its data */
  ASSERT_EQ(0, blocks_held);
  memory = memory_of(text, sizeof text);
  counting_reset(SIZE_MAX);
  dots_mdl_open_source(&source, counting, input, &header);
  result = dots_mdl_get_frame(&source, &frame);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_OUTSIDE_GRID, result.code);
  ASSERT_EQ(sizeof text - 1, result.offset);
  ASSERT_EQ(3, allocations);
  ASSERT_EQ(0, blocks_held);
  PASS();
}

/* Each allocation fails in turn: NO_MEMORY, final, nothing held. */
TEST model_no_memory(void) {
  static const unsigned char stream[] = {
      0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00, 0x01, 0x10, 0x04,
      0x0A, 0x01, 0x02, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x01,
      0x00, 0xFE, 0x02, 0x00, 0x00, 0x02, 0x48, 0x69, 0xFD, 0xFF};
  size_t fail;

  for (fail = 0; fail < 5; ++fail) {
    struct memory memory = memory_of(stream, sizeof stream);
    struct dots_mdl_input input = {memory_get, &memory};
    struct dots_mdl_source source;
    struct dots_header header;
    struct dots_mdl_frame *frame;

    counting_reset(fail);
    dots_mdl_open_source(&source, counting, input, &header);
    ASSERT_EQ(DOTS_MDL_NO_MEMORY, dots_mdl_get_frame(&source, &frame).status);
    ASSERT_EQ(NULL, frame);
    ASSERT_EQ(0, blocks_held);
    ASSERT_EQ(DOTS_MDL_NO_MEMORY, dots_mdl_get_frame(&source, &frame).status);
  }
  PASS();
}

TEST model_io_failed(void) {
  struct memory memory = memory_of(hi_stream, sizeof hi_stream);
  struct dots_mdl_input input = {memory_get, &memory};
  struct dots_mdl_output output = {memory_put, &memory};
  struct dots_header header = hi_header();
  struct dots_mdl_source source;
  struct dots_mdl_sink sink;
  struct dots_mdl_frame *frame;
  struct dots_mdl_frame empty = {false, {NULL}, {NULL}};

  memory.fail_at = 15;
  counting_reset(SIZE_MAX);
  dots_mdl_open_source(&source, counting, input, &header);
  ASSERT_EQ(DOTS_MDL_IO_FAILED, dots_mdl_get_frame(&source, &frame).status);
  ASSERT_EQ(0, blocks_held);
  ASSERT_EQ(DOTS_MDL_IO_FAILED, dots_mdl_get_frame(&source, &frame).status);
  memory = memory_of(NULL, 0);
  memory.fail_at = 12;
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_open_sink(&sink, output, &header).status);
  ASSERT_EQ(DOTS_MDL_IO_FAILED, dots_mdl_put_frame(&sink, &empty).status);
  ASSERT_EQ(DOTS_MDL_IO_FAILED, dots_mdl_close_sink(&sink).status);
  PASS();
}

/* The writer's code, final; the frame partly written. */
TEST model_sink_ill_formed(void) {
  struct memory memory = memory_of(NULL, 0);
  struct dots_mdl_output output = {memory_put, &memory};
  struct dots_header header = hi_header();
  struct dots_mdl_sink sink;
  unsigned char text[] = "Hi";
  struct dots_mdl_run hi = {{0, 0}, 2, text, NULL};
  struct dots_mdl_frame frame = {true, {NULL}, {&hi}};
  struct dots_mdl_result result;

  header.flags = 0;
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_open_sink(&sink, output, &header).status);
  result = dots_mdl_put_frame(&sink, &frame);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_UNEXPECTED_TEXT, result.code);
  ASSERT_EQ(13, memory.size); /* the header, clear, end of blocks */
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, dots_mdl_close_sink(&sink).status);
  header.version.minor = 0;
  result = dots_mdl_open_sink(&sink, output, &header);
  ASSERT_EQ(DOTS_MDL_ILL_FORMED, result.status);
  ASSERT_EQ(DOTS_UNSUPPORTED_VERSION, result.code);
  PASS();
}

TEST model_compose(void) {
  struct dots_header header = hi_header();
  struct dots_evt_composer composer;
  struct dots_cell cells[64];
  unsigned char blocks[] = {0x39, 0xFF};
  unsigned char text[] = "Hi";
  struct dots_mdl_run hi = {{14, 3}, 2, text, NULL};
  struct dots_mdl_run dots = {{1, 0}, 2, blocks, NULL};
  struct dots_mdl_frame first = {true, {&dots}, {&hi}};
  struct dots_mdl_frame second = {false, {NULL}, {NULL}};

  dots_evt_composer_init(&composer, &header, cells, 64);
  dots_mdl_compose(&composer, &first);
  dots_mdl_compose(&composer, &second);
  ASSERT_EQ(0x00, cells[0].block);
  ASSERT_EQ(0x39, cells[1].block);
  ASSERT_EQ(0xFF, cells[2].block);
  ASSERT_EQ('H', cells[62].character);
  ASSERT_EQ('i', cells[63].character);
  first.clear = true;
  first.blocks.runs = NULL;
  first.text.runs = NULL;
  dots_mdl_compose(&composer, &first);
  ASSERT_EQ(0x00, cells[1].block);
  ASSERT_EQ(0, cells[63].character);
  PASS();
}

TEST model_stdio(void) {
  FILE *file = tmpfile();
  struct dots_mdl_input input = {dots_mdl_stdio_get, NULL};
  struct dots_mdl_output output = {dots_mdl_stdio_put, NULL};
  struct dots_mdl_allocator allocator = {malloc, free};
  struct dots_header header = hi_header();
  struct dots_mdl_sink sink;
  struct dots_mdl_source source;
  struct dots_mdl_frame *frame;
  struct dots_mdl_frame empty = {true, {NULL}, {NULL}};

  if (file == NULL) {
    SKIPm("no temporary file");
  }
  input.handle = file;
  output.handle = file;
  dots_mdl_open_sink(&sink, output, &header);
  dots_mdl_put_frame(&sink, &empty);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_close_sink(&sink).status);
  rewind(file);
  ASSERT_EQ(DOTS_MDL_OK,
            dots_mdl_open_source(&source, allocator, input, &header).status);
  ASSERT_EQ(DOTS_MDL_OK, dots_mdl_get_frame(&source, &frame).status);
  ASSERT(frame->clear);
  dots_mdl_release_frame(&source, frame);
  ASSERT_EQ(DOTS_MDL_END, dots_mdl_get_frame(&source, &frame).status);
  fclose(file);
  PASS();
}

SUITE(model) {
  RUN_TEST(model_read);
  RUN_TEST(model_write);
  RUN_TEST(model_round_trip);
  RUN_TEST(model_ill_formed);
  RUN_TEST(model_run_past_row);
  RUN_TEST(model_no_memory);
  RUN_TEST(model_io_failed);
  RUN_TEST(model_sink_ill_formed);
  RUN_TEST(model_compose);
  RUN_TEST(model_stdio);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(reader);
  RUN_SUITE(writer);
  RUN_SUITE(leb128);
  RUN_SUITE(utf8);
  RUN_SUITE(composer);
  RUN_SUITE(model);
  GREATEST_MAIN_END();
}
