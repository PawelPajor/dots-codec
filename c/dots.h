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

#include <stddef.h>
#include <stdint.h>

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

/*
 * Reader
 *
 * An incremental (push) parser. The caller feeds the stream in chunks
 * of any size, in order, and receives one event per call. The reader
 * allocates nothing and never holds the stream.
 *
 *   struct dots_reader reader;
 *   struct dots_event event;
 *   dots_reader_init(&reader);
 *   while (size > 0) {
 *     size_t used = dots_reader_feed(&reader, data, size, &event);
 *     data += used;
 *     size -= used;
 *     ... handle event, stop on END_OF_STREAM or ERROR ...
 *   }
 *   dots_reader_finish(&reader, &event);   at the end of input
 */

enum dots_event_type {
  DOTS_EVENT_NONE,          /* all input used, more needed */
  DOTS_EVENT_HEADER,        /* the header: event.header */
  DOTS_EVENT_FRAME,         /* the start of a frame: event.clear */
  DOTS_EVENT_RUN,           /* the start of a run: event.run */
  DOTS_EVENT_DATA,          /* octets of the current run: event.data */
  DOTS_EVENT_END_OF_BLOCKS, /* the end of the block layer */
  DOTS_EVENT_END_OF_TEXT,   /* the end of the text layer and the frame */
  DOTS_EVENT_END_OF_STREAM, /* the stream is well-formed */
  DOTS_EVENT_ERROR          /* the stream is not: event.code */
};

struct dots_header {
  uint64_t major, minor, update;
  uint64_t flags;
  uint64_t columns, rows;
  uint64_t frame_rate;
};

struct dots_run {
  int text; /* 1 in the text layer, 0 in the block layer */
  uint64_t x, y;
  uint64_t count;
};

/*
 * A piece of the data of the current run. It points into the input of
 * the call or into the reader, and is valid until the next call. In
 * the text layer it holds only whole, well-formed UTF-8 sequences.
 */
struct dots_data {
  const unsigned char *octets;
  size_t size;
};

struct dots_event {
  enum dots_event_type type;
  uint64_t offset; /* the offset of the event, or of the error */
  enum dots_code code;
  int clear; /* the frame starts with a clear */
  struct dots_header header;
  struct dots_run run;
  struct dots_data data;
};

/* The reader's state. Its members are private. */
struct dots_reader {
  int state;
  uint64_t offset;
  struct dots_header header;
  struct dots_run run;
  uint64_t value, start;
  unsigned shift;
  uint64_t remaining, cells;
  unsigned char utf8[4];
  unsigned utf8_size, utf8_need;
  struct dots_event error;
};

void dots_reader_init(struct dots_reader *reader);

/*
 * Reads from the size octets at data, up to the next event, and
 * returns the number of octets used. After DOTS_EVENT_END_OF_STREAM it
 * uses all input and ignores it (STR-2). After DOTS_EVENT_ERROR it uses
 * nothing and returns the same error.
 */
size_t dots_reader_feed(struct dots_reader *reader, const unsigned char *data,
                        size_t size, struct dots_event *event);

/*
 * Ends the input. Gives DOTS_EVENT_END_OF_STREAM if the stream ended,
 * otherwise the error: TRUNCATED at the end of the stream, or the
 * error already found.
 */
void dots_reader_finish(struct dots_reader *reader, struct dots_event *event);

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
