/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 */

/**
 * @file dots.h
 * Reader and writer of DOTS streams, in C99.
 *
 * Include this header wherever it is needed. In exactly one source
 * file, define DOTS_IMPLEMENTATION before including it. Both parts
 * compile as C++11 and later too.
 *
 * Names of the interface start with `dots_`, or `DOTS_` for macros and
 * constants.
 *
 * Names of the implementation start with `dots__` or `DOTS__`.
 * They are not part of the interface, and may change at any time.
 *
 * The private state of a struct `dots_x` is its member `prv`, of type
 * `struct dots_x_prv`.
 */

#ifndef DOTS_H
#define DOTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The major version a stream carries (HDR-1). */
#define DOTS_SPEC_MAJOR 0

/** The minor version a stream carries (HDR-1). */
#define DOTS_SPEC_MINOR 1

/** Result codes, with the values of the specification. */
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

/**
 * The name of a code, as the specification writes it.
 *
 * @param code a code
 * @return its name, or 0 if the code is unknown
 */
const char *dots_code_name(enum dots_code code);

/**
 * @defgroup reader Reader
 *
 * An incremental (push) parser. The caller feeds the stream in chunks
 * of any size, in order, and receives one event per call. The reader
 * allocates nothing and never holds the stream.
 *
 * @code
 * struct dots_reader reader;
 * struct dots_event event;
 *
 * dots_reader_init(&reader);
 * while (size > 0) {
 *   size_t used = dots_reader_feed(&reader, data, size, &event);
 *
 *   data += used;
 *   size -= used;
 *   ... handle event, stop on END_OF_STREAM or ERROR ...
 * }
 * dots_reader_finish(&reader, &event);   at the end of input
 * @endcode
 *
 * @{
 */

/** The type of an event. */
enum dots_event_type {
  DOTS_EVENT_NONE,          /**< all input used, more needed */
  DOTS_EVENT_HEADER,        /**< the header: event.header */
  DOTS_EVENT_FRAME,         /**< the start of a frame: event.clear */
  DOTS_EVENT_RUN,           /**< the start of a run: event.run */
  DOTS_EVENT_CHUNK,         /**< octets of the current run: event.chunk */
  DOTS_EVENT_END_OF_BLOCKS, /**< the end of the block layer */
  DOTS_EVENT_END_OF_TEXT,   /**< the end of the text layer and the frame */
  DOTS_EVENT_END_OF_STREAM, /**< the stream is well-formed */
  DOTS_EVENT_ERROR          /**< the stream is not: event.code */
};

/** The header of a stream, decoded. */
struct dots_header {
  uint64_t major;
  uint64_t minor;
  uint64_t update;
  uint64_t flags;
  uint64_t columns;
  uint64_t rows;
  uint64_t frame_rate;
};

/** The start of a run. */
struct dots_run {
  int text; /**< 1 in the text layer, 0 in the block layer */
  uint64_t x;
  uint64_t y;
  uint64_t count;
};

/**
 * A piece of the data of the current run. It points into the input of
 * the call or into the reader, and is valid until the next call. In
 * the text layer it holds only whole, well-formed UTF-8 sequences.
 */
struct dots_chunk {
  const unsigned char *octets;
  size_t size;
};

/** An event of the reader. Its type says which members hold it. */
struct dots_event {
  enum dots_event_type type;
  uint64_t offset; /**< the offset of the event, or of the error */
  enum dots_code code;
  int clear; /**< the frame starts with a clear */
  struct dots_header header;
  struct dots_run run;
  struct dots_chunk chunk;
};

/** The states of a reader: the part of the stream it reads next. */
enum dots__state {
  DOTS__STATE_FOURCC,
  DOTS__STATE_MAJOR,
  DOTS__STATE_MINOR,
  DOTS__STATE_UPDATE,
  DOTS__STATE_FLAGS,
  DOTS__STATE_COLUMNS,
  DOTS__STATE_ROWS,
  DOTS__STATE_FRAME_RATE,
  DOTS__STATE_FRAMES,
  DOTS__STATE_ERROR
};

/** The state of a reader: private, it may change at any time. */
struct dots_reader_prv {
  enum dots__state state;
  uint64_t offset;
  struct dots_header header;
  struct dots_run run;
  uint64_t value;
  uint64_t start;
  unsigned shift;
  int overflow;
  uint64_t remaining;
  uint64_t cells;
  unsigned char utf8[4];
  unsigned utf8_size;
  unsigned utf8_need;
  struct dots_event error;
};

/** A reader. */
struct dots_reader {
  struct dots_reader_prv prv; /**< private */
};

/**
 * Starts reading a stream.
 *
 * @param reader the reader
 */
void dots_reader_init(struct dots_reader *reader);

/**
 * Reads the input up to the next event. After DOTS_EVENT_END_OF_STREAM
 * it uses all input and ignores it (STR-2). After DOTS_EVENT_ERROR it
 * uses nothing and gives the same error.
 *
 * @param reader the reader
 * @param data the next octets of the stream
 * @param size the number of octets at data
 * @param[out] event the event
 * @return the number of octets used
 */
size_t dots_reader_feed(struct dots_reader *reader, const unsigned char *data,
                        size_t size, struct dots_event *event);

/**
 * Ends the input. Gives DOTS_EVENT_END_OF_STREAM if the stream ended,
 * otherwise the error: TRUNCATED at the end of the stream, or the
 * error already found.
 *
 * @param reader the reader
 * @param[out] event the event
 */
void dots_reader_finish(struct dots_reader *reader, struct dots_event *event);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* DOTS_H */

#if defined(DOTS_IMPLEMENTATION) && !defined(DOTS_IMPLEMENTATION_DONE)
#define DOTS_IMPLEMENTATION_DONE

/** An initializer of a struct with all members zero, in C and C++. */
#ifdef __cplusplus
#define DOTS__ZERO                                                             \
  {                                                                            \
  }
#else
#define DOTS__ZERO {0}
#endif

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

/** What an octet does to a leb128. */
enum dots__leb128_result {
  DOTS__LEB128_MORE,      /**< the leb128 continues */
  DOTS__LEB128_DONE,      /**< the leb128 ends: its value is prv->value */
  DOTS__LEB128_MALFORMED, /**< the octet is a 0x00 tail */
  DOTS__LEB128_OVERFLOW   /**< the leb128 ends, above 2^64 - 1 */
};

/**
 * Reads one octet of a leb128 (INT-1). Its first octet sets
 * prv->start. A value above 2^64 - 1 is known only at the last
 * octet: before it, a 0x00 tail or the end of the stream makes the
 * octets no leb128 at all.
 *
 * @param prv the reader's state
 * @param octet the octet
 * @return what the octet does to the leb128
 */
static enum dots__leb128_result dots__leb128(struct dots_reader_prv *prv,
                                             unsigned char octet) {
  uint64_t bits = octet & 0x7F;
  enum dots__leb128_result result;

  if (prv->shift == 0) {
    prv->start = prv->offset;
    prv->value = 0;
    prv->overflow = 0;
  }
  if (prv->shift < 64) {
    if (prv->shift == 63 && bits > 1) {
      prv->overflow = 1;
    }
    prv->value |= bits << prv->shift;
  } else if (bits != 0) {
    prv->overflow = 1;
  }
  if (octet & 0x80) {
    if (prv->shift < 64) {
      prv->shift += 7;
    }
    return DOTS__LEB128_MORE;
  }
  if (octet == 0 && prv->shift != 0) {
    return DOTS__LEB128_MALFORMED;
  }
  result = prv->overflow ? DOTS__LEB128_OVERFLOW : DOTS__LEB128_DONE;
  prv->shift = 0;
  return result;
}

/**
 * Gives an error, from now on.
 *
 * @param prv the reader's state
 * @param code the code of the error
 * @param offset the offset of the error
 * @param used the number of octets used by the call
 * @param[out] event the error
 * @return used
 */
static size_t dots__fail(struct dots_reader_prv *prv, enum dots_code code,
                         uint64_t offset, size_t used,
                         struct dots_event *event) {
  prv->state = DOTS__STATE_ERROR;
  prv->error.type = DOTS_EVENT_ERROR;
  prv->error.code = code;
  prv->error.offset = offset;
  *event = prv->error;
  return used;
}

void dots_reader_init(struct dots_reader *reader) {
  static const struct dots_reader initial = DOTS__ZERO;

  *reader = initial;
}

size_t dots_reader_feed(struct dots_reader *reader, const unsigned char *data,
                        size_t size, struct dots_event *event) {
  static const unsigned char fourcc[4] = {0x64, 0x6F, 0x74, 0x73};
  static const struct dots_event none = DOTS__ZERO;
  struct dots_reader_prv *prv = &reader->prv;
  size_t used = 0;

  if (prv->state == DOTS__STATE_ERROR) {
    *event = prv->error;
    return 0;
  }
  *event = none;
  /* Frames are not read yet: the rest of the stream is ignored. */
  if (prv->state == DOTS__STATE_FRAMES) {
    prv->offset += size;
    return size;
  }
  while (used < size) {
    unsigned char octet = data[used];
    uint64_t value;
    enum dots__leb128_result result;

    if (prv->state == DOTS__STATE_FOURCC) {
      if (octet != fourcc[prv->offset]) {
        return dots__fail(prv, DOTS_MALFORMED, prv->offset, used, event);
      }
      ++prv->offset;
      ++used;
      if (prv->offset == sizeof fourcc) {
        prv->state = DOTS__STATE_MAJOR;
      }
      continue;
    }
    result = dots__leb128(prv, octet);
    if (result == DOTS__LEB128_MALFORMED) {
      return dots__fail(prv, DOTS_MALFORMED, prv->offset, used, event);
    }
    if (result == DOTS__LEB128_OVERFLOW) {
      return dots__fail(prv, DOTS_OVERFLOW, prv->start, used, event);
    }
    ++prv->offset;
    ++used;
    if (result == DOTS__LEB128_MORE) {
      continue;
    }
    value = prv->value;
    switch (prv->state) {
    case DOTS__STATE_FOURCC:
      break; /* already handled above, listed here for completeness */
    case DOTS__STATE_MAJOR:
      if (value != DOTS_SPEC_MAJOR) {
        return dots__fail(prv, DOTS_UNSUPPORTED_VERSION, prv->start, used,
                          event);
      }
      prv->header.major = value;
      prv->state = DOTS__STATE_MINOR;
      break;
    case DOTS__STATE_MINOR:
      if (value != DOTS_SPEC_MINOR) {
        return dots__fail(prv, DOTS_UNSUPPORTED_VERSION, prv->start, used,
                          event);
      }
      prv->header.minor = value;
      prv->state = DOTS__STATE_UPDATE;
      break;
    case DOTS__STATE_UPDATE:
      prv->header.update = value;
      prv->state = DOTS__STATE_FLAGS;
      break;
    case DOTS__STATE_FLAGS:
      if (value > 1) {
        return dots__fail(prv, DOTS_INVALID_VALUE, prv->start, used, event);
      }
      prv->header.flags = value;
      prv->state = DOTS__STATE_COLUMNS;
      break;
    case DOTS__STATE_COLUMNS:
      if (value == 0) {
        return dots__fail(prv, DOTS_INVALID_VALUE, prv->start, used, event);
      }
      prv->header.columns = value;
      prv->state = DOTS__STATE_ROWS;
      break;
    case DOTS__STATE_ROWS:
      if (value == 0) {
        return dots__fail(prv, DOTS_INVALID_VALUE, prv->start, used, event);
      }
      prv->header.rows = value;
      prv->state = DOTS__STATE_FRAME_RATE;
      break;
    case DOTS__STATE_FRAME_RATE:
      prv->header.frame_rate = value;
      prv->state = DOTS__STATE_FRAMES;
      event->type = DOTS_EVENT_HEADER;
      event->offset = 0;
      event->header = prv->header;
      return used;
    case DOTS__STATE_FRAMES:
      break; /* not implemented yet */
    case DOTS__STATE_ERROR:
      break; /* not reached: read before the leb128 */
    }
  }
  return used;
}

void dots_reader_finish(struct dots_reader *reader, struct dots_event *event) {
  struct dots_reader_prv *prv = &reader->prv;

  if (prv->state == DOTS__STATE_ERROR) {
    *event = prv->error;
    return;
  }
  event->type = DOTS_EVENT_ERROR;
  event->code = DOTS_TRUNCATED;
  event->offset = prv->offset;
}

#endif /* DOTS_IMPLEMENTATION */
