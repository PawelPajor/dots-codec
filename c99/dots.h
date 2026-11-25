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

/**
 * @mainpage DOTS in C99
 *
 * A reader and a writer of
 * [DOTS](https://github.com/PawelPajor/dots-stream) streams in one
 * header, dots.h: no dependencies, no allocation. See dots.h for how
 * to include it.
 *
 * Both directions use the same @ref events "events": a header, then
 * frames, each a block layer and a text layer of runs, then the end of
 * the stream.
 *
 * To play a stream:
 *
 * 1. Feed its octets to a @ref reader "reader", which gives one event
 *    per call.
 * 2. On the header, start a @ref composer "composer" with cells for
 *    columns * rows.
 * 3. Give it every later event. On each end of text, the cells hold
 *    the frame to show: a cell's character, else its block, as
 *    dots_braille.
 *
 * @code
 * dots_reader_init(&reader);
 * while (size > 0) {
 *   size_t used = dots_reader_feed(&reader, data, size, &event);
 *
 *   data += used;
 *   size -= used;
 *   if (event.type == DOTS_EVENT_HEADER) {
 *     dots_composer_init(&composer, &event.header, cells, count);
 *   } else {
 *     dots_composer_apply(&composer, &event);
 *   }
 *   if (event.type == DOTS_EVENT_END_OF_TEXT) {
 *     ... show cells ...
 *   }
 *   ... stop on DOTS_EVENT_END_OF_STREAM or DOTS_EVENT_ERROR ...
 * }
 * dots_reader_finish(&reader, &event);
 * @endcode
 *
 * To make a stream, give the same events in order to a
 * @ref writer "writer", which encodes each one or refuses it.
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
 * @defgroup events Events
 *
 * The parts of a stream, in stream order: what the reader gives and
 * the writer takes.
 *
 * @{
 */

/** The type of an event. */
enum dots_event_type {
  DOTS_EVENT_NONE,          /**< no event */
  DOTS_EVENT_HEADER,        /**< the header: event.header */
  DOTS_EVENT_FRAME,         /**< the start of a frame: event.clear */
  DOTS_EVENT_RUN,           /**< the start of a run: event.run */
  DOTS_EVENT_CHUNK,         /**< octets of the current run: event.chunk */
  DOTS_EVENT_END_OF_BLOCKS, /**< the end of the block layer */
  DOTS_EVENT_END_OF_TEXT,   /**< the end of the text layer and the frame */
  DOTS_EVENT_END_OF_STREAM, /**< the end of the stream */
  DOTS_EVENT_ERROR          /**< an error: event.code */
};

/** The header of a stream. */
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

/** A piece of the data of the current run. */
struct dots_chunk {
  const unsigned char *octets;
  size_t size;
};

/** An event. Its type says which members hold it. */
struct dots_event {
  enum dots_event_type type;
  uint64_t offset; /**< the offset of the event, or of the error */
  enum dots_code code;
  int clear; /**< the frame starts with a clear */
  struct dots_header header;
  struct dots_run run;
  struct dots_chunk chunk;
};

/** @} */

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
  DOTS__STATE_FRAME,  /**< a frame, or end-of-stream */
  DOTS__STATE_BLOCKS, /**< a run, or end-of-blocks */
  DOTS__STATE_TEXT,   /**< a run, or end-of-text */
  DOTS__STATE_X,
  DOTS__STATE_Y,
  DOTS__STATE_COUNT,
  DOTS__STATE_DATA,
  DOTS__STATE_END, /**< nothing: the stream has ended */
  DOTS__STATE_ERROR
};

/** The run being read. */
struct dots__run {
  struct dots_run head; /**< its layer, position and count */
  uint64_t offset;      /**< its offset */
  uint64_t data_left;   /**< its data octets not yet read */
  uint64_t cells_left;  /**< the cells left in its row */
};

/** A leb128, read one octet at a time. */
struct dots__leb128_reader {
  uint64_t value;  /**< its value, so far */
  uint64_t offset; /**< its offset */
  unsigned shift;  /**< the shift of its next octet; 0 before its first */
  int overflow;    /**< its value is above 2^64 - 1 */
};

/** What an octet does to a leb128. */
enum dots__leb128_reader_result {
  DOTS__LEB128_READER_MORE,      /**< the leb128 continues */
  DOTS__LEB128_READER_DONE,      /**< the leb128 ends: its value is in value */
  DOTS__LEB128_READER_MALFORMED, /**< the octet is a 0x00 tail */
  DOTS__LEB128_READER_OVERFLOW   /**< the leb128 ends, above 2^64 - 1 */
};

/**
 * A UTF-8 sequence, read one octet at a time. It is in progress while
 * size < length, and complete when size == length.
 */
struct dots__utf8 {
  unsigned char octets[4]; /**< its octets read */
  unsigned size;           /**< their number */
  unsigned length;         /**< its length, set by its first octet */
  uint64_t offset;         /**< its offset */
};

/** What an octet does to a UTF-8 sequence. */
enum dots__utf8_result {
  DOTS__UTF8_MORE,   /**< the sequence continues */
  DOTS__UTF8_DONE,   /**< the sequence ends: it is in octets */
  DOTS__UTF8_INVALID /**< the sequence is ill-formed */
};

/** The state of a reader: private, it may change at any time. */
struct dots__reader_prv {
  enum dots__state state;
  uint64_t offset;
  struct dots_header header;
  struct dots__run run;
  struct dots__leb128_reader leb128;
  struct dots__utf8 utf8;
  struct dots_event error;
};

/** A reader. */
struct dots_reader {
  struct dots__reader_prv prv; /**< private */
};

/**
 * Starts reading a stream.
 *
 * @param reader the reader
 */
void dots_reader_init(struct dots_reader *reader);

/**
 * Reads the input up to the next event. DOTS_EVENT_NONE means all
 * input is used and more is needed. A chunk points into the input or
 * into the reader, and is valid until the next call; in the text layer
 * it holds only whole, well-formed UTF-8 sequences. After
 * DOTS_EVENT_END_OF_STREAM, which means the stream is well-formed, it
 * uses all input and ignores it (STR-2). After DOTS_EVENT_ERROR it uses
 * nothing and gives the same error.
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

/**
 * @defgroup writer Writer
 *
 * Encodes events into a stream, one call per event, in buffers the
 * caller provides. It refuses any event that would make the stream
 * ill-formed (CNF-1), and then writes nothing and keeps its state. The
 * writer allocates nothing and never holds the stream.
 *
 * @code
 * struct dots_writer writer;
 * struct dots_writer_result result;
 *
 * dots_writer_init(&writer);
 * for each event, up to END_OF_STREAM:
 *   result = dots_writer_put(&writer, &event, out, capacity);
 *   ... on DOTS_WRITER_OK, out holds result.size octets of the stream ...
 * @endcode
 *
 * @{
 */

/** The most octets an event other than a chunk is written as. */
#define DOTS_WRITER_MAX 74

/** What the writer did with an event. */
enum dots_writer_status {
  DOTS_WRITER_OK,           /**< the event is written */
  DOTS_WRITER_ILL_FORMED,   /**< it breaks a stream requirement: code */
  DOTS_WRITER_OUT_OF_ORDER, /**< the stream has no such event next */
  DOTS_WRITER_PAST_COUNT,   /**< a chunk past the count of its run */
  DOTS_WRITER_NO_SPACE      /**< its octets do not fit the capacity */
};

/** The result of a call of the writer. */
struct dots_writer_result {
  enum dots_writer_status status;
  enum dots_code code; /**< the code of the requirement, if ILL_FORMED */
  size_t size;         /**< the number of octets written */
};

/** The state of a writer: private, it may change at any time. */
struct dots__writer_prv {
  struct dots_reader reader; /**< has read every octet written */
  int frame; /**< a frame without a clear has started, none of it written */
};

/** A writer. */
struct dots_writer {
  struct dots__writer_prv prv; /**< private */
};

/**
 * Starts writing a stream.
 *
 * @param writer the writer
 */
void dots_writer_init(struct dots_writer *writer);

/**
 * Writes an event. A frame without a clear is written as nothing: its
 * first octet belongs to its block layer. A chunk of the text layer may
 * end inside a UTF-8 sequence, which the next chunk continues. A run
 * or a chunk after which no data can end the run well-formed is
 * refused: blocks past the row, more than 4 octets per cell left of
 * text, or too few octets for the sequence in progress. The
 * offset, and the members the type of the event does not name, are
 * ignored.
 *
 * @param writer the writer
 * @param event the event
 * @param[out] out where its octets go
 * @param capacity the number of octets out holds: DOTS_WRITER_MAX for
 *        any event but a chunk, which needs its size
 * @return DOTS_WRITER_OK and the octets written, or a refusal
 */
struct dots_writer_result dots_writer_put(struct dots_writer *writer,
                                          const struct dots_event *event,
                                          unsigned char *out, size_t capacity);

/** @} */

/**
 * @defgroup composer Composer
 *
 * Applies the events of a reader to the state of the grid (CMP-1 to
 * CMP-3), in cells the caller provides. The composer allocates
 * nothing. The surface of a cell is its character if it has one,
 * otherwise its block, which dots_braille can show.
 *
 * @code
 * struct dots_composer composer;
 *
 * on DOTS_EVENT_HEADER:
 *   if (!dots_composer_init(&composer, &event.header, cells, count)) {
 *     ... the grid has more cells than count ...
 *   }
 * on any other event:
 *   dots_composer_apply(&composer, &event);
 * on DOTS_EVENT_END_OF_TEXT:
 *   ... cells hold the state left by the frame ...
 * @endcode
 *
 * @{
 */

/** A cell of the state: cell (x, y) is at index y * columns + x. */
struct dots_cell {
  uint32_t character; /**< its character, or 0 if it has none */
  unsigned char block;
};

/** The state of a composer: private, it may change at any time. */
struct dots__composer_prv {
  struct dots_cell *cells;
  size_t columns;
  size_t rows;
  size_t next; /**< the index of the next cell of the run */
  int text;    /**< the run is in the text layer */
};

/** A composer. */
struct dots_composer {
  struct dots__composer_prv prv; /**< private */
};

/**
 * Starts composing a stream, from the initial state.
 *
 * @param composer the composer
 * @param header the header of the stream
 * @param cells the cells of the state
 * @param count the number of cells at cells
 * @return 1, or 0 if count is less than columns * rows
 */
int dots_composer_init(struct dots_composer *composer,
                       const struct dots_header *header,
                       struct dots_cell *cells, size_t count);

/**
 * Applies an event of the reader that read the header given to
 * dots_composer_init. A frame with a clear and a chunk change the
 * state, a run says where its chunks go, other events do nothing.
 *
 * @param composer the composer
 * @param event the event
 */
void dots_composer_apply(struct dots_composer *composer,
                         const struct dots_event *event);

/**
 * The Braille pattern of a block (Section 5 of dots-content.md).
 *
 * @param block a block
 * @return its code point, from U+2800 to U+28FF
 */
uint32_t dots_braille(unsigned char block);

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

/**
 * Adds one octet to a leb128 (INT-1). A value above 2^64 - 1 is known
 * only at the last octet: before it, a 0x00 tail or the end of the
 * stream makes the octets no leb128 at all.
 *
 * @param leb128 the leb128
 * @param octet the octet
 * @param offset the offset of the octet
 * @return what the octet does to the leb128
 */
static enum dots__leb128_reader_result
dots__leb128_read(struct dots__leb128_reader *leb128, unsigned char octet,
                  uint64_t offset) {
  uint64_t bits = octet & 0x7F;
  enum dots__leb128_reader_result result;

  if (leb128->shift == 0) {
    leb128->offset = offset;
    leb128->value = 0;
    leb128->overflow = 0;
  }
  if (leb128->shift < 64) {
    if (leb128->shift == 63 && bits > 1) {
      leb128->overflow = 1;
    }
    leb128->value |= bits << leb128->shift;
  } else if (bits != 0) {
    leb128->overflow = 1;
  }
  if (octet & 0x80) {
    if (leb128->shift < 64) {
      leb128->shift += 7;
    }
    return DOTS__LEB128_READER_MORE;
  }
  if (octet == 0 && leb128->shift != 0) {
    return DOTS__LEB128_READER_MALFORMED;
  }
  result = leb128->overflow ? DOTS__LEB128_READER_OVERFLOW
                            : DOTS__LEB128_READER_DONE;
  leb128->shift = 0;
  return result;
}

/** The tokens (Section 4.2). */
enum dots__token {
  DOTS__CLEAR = 0x01,
  DOTS__START_OF_RUN = 0x02,
  DOTS__END_OF_BLOCKS = 0xFE,
  DOTS__END_OF_TEXT = 0xFD,
  DOTS__END_OF_STREAM = 0xFF
};

/** The fourcc (Section 5). */
static const unsigned char dots__fourcc[4] = {0x64, 0x6F, 0x74, 0x73};

/**
 * Gives an error, from now on.
 *
 * @param prv the reader's state
 * @param code the code of the error
 * @param offset the offset of the error
 * @param[out] event the error
 * @return 0, the number of octets used
 */
static size_t dots__fail(struct dots__reader_prv *prv, enum dots_code code,
                         uint64_t offset, struct dots_event *event) {
  prv->state = DOTS__STATE_ERROR;
  prv->error.type = DOTS_EVENT_ERROR;
  prv->error.code = code;
  prv->error.offset = offset;
  *event = prv->error;
  return 0;
}

/**
 * Uses a token and gives its event.
 *
 * @param prv the reader's state
 * @param type the type of the event
 * @param next the next state
 * @param[out] event the event
 * @return 1, the number of octets used
 */
static size_t dots__token(struct dots__reader_prv *prv,
                          enum dots_event_type type, enum dots__state next,
                          struct dots_event *event) {
  event->type = type;
  event->offset = prv->offset;
  prv->state = next;
  ++prv->offset;
  return 1;
}

/**
 * The state after the current run: its layer.
 *
 * @param prv the reader's state
 * @return the state
 */
static enum dots__state dots__layer(const struct dots__reader_prv *prv) {
  return prv->run.head.text ? DOTS__STATE_TEXT : DOTS__STATE_BLOCKS;
}

/**
 * Reads an octet of the fourcc.
 *
 * @param prv the reader's state
 * @param octet the next octet of the input
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_fourcc(struct dots__reader_prv *prv,
                                unsigned char octet, struct dots_event *event) {
  if (octet != dots__fourcc[prv->offset]) {
    return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  prv->state = prv->offset + 1 == sizeof dots__fourcc ? DOTS__STATE_MAJOR
                                                      : DOTS__STATE_FOURCC;
  ++prv->offset;
  return 1;
}

/**
 * Reads an octet of a leb128 field: the header, or the start of a run.
 *
 * @param prv the reader's state
 * @param octet the next octet of the input
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_field(struct dots__reader_prv *prv,
                               unsigned char octet, struct dots_event *event) {
  enum dots__leb128_reader_result result =
      dots__leb128_read(&prv->leb128, octet, prv->offset);
  uint64_t value = prv->leb128.value;

  if (result == DOTS__LEB128_READER_MALFORMED) {
    return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  if (result == DOTS__LEB128_READER_OVERFLOW) {
    return dots__fail(prv, DOTS_OVERFLOW, prv->leb128.offset, event);
  }
  ++prv->offset;
  if (result == DOTS__LEB128_READER_MORE) {
    return 1;
  }
  switch (prv->state) {
  case DOTS__STATE_MAJOR:
    if (value != DOTS_SPEC_MAJOR) {
      return dots__fail(prv, DOTS_UNSUPPORTED_VERSION, prv->leb128.offset,
                        event);
    }
    prv->header.major = value;
    prv->state = DOTS__STATE_MINOR;
    break;
  case DOTS__STATE_MINOR:
    if (value != DOTS_SPEC_MINOR) {
      return dots__fail(prv, DOTS_UNSUPPORTED_VERSION, prv->leb128.offset,
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
      return dots__fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset, event);
    }
    prv->header.flags = value;
    prv->state = DOTS__STATE_COLUMNS;
    break;
  case DOTS__STATE_COLUMNS:
    if (value == 0) {
      return dots__fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset, event);
    }
    prv->header.columns = value;
    prv->state = DOTS__STATE_ROWS;
    break;
  case DOTS__STATE_ROWS:
    if (value == 0) {
      return dots__fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset, event);
    }
    prv->header.rows = value;
    prv->state = DOTS__STATE_FRAME_RATE;
    break;
  case DOTS__STATE_FRAME_RATE:
    prv->header.frame_rate = value;
    prv->state = DOTS__STATE_FRAME;
    event->type = DOTS_EVENT_HEADER;
    event->offset = 0;
    event->header = prv->header;
    break;
  case DOTS__STATE_X:
    if (value >= prv->header.columns) {
      return dots__fail(prv, DOTS_OUTSIDE_GRID, prv->leb128.offset, event);
    }
    prv->run.head.x = value;
    prv->run.cells_left = prv->header.columns - value;
    prv->state = DOTS__STATE_Y;
    break;
  case DOTS__STATE_Y:
    if (value >= prv->header.rows) {
      return dots__fail(prv, DOTS_OUTSIDE_GRID, prv->leb128.offset, event);
    }
    prv->run.head.y = value;
    prv->state = DOTS__STATE_COUNT;
    break;
  case DOTS__STATE_COUNT:
    prv->run.head.count = value;
    prv->run.data_left = value;
    prv->state = value == 0 ? dots__layer(prv) : DOTS__STATE_DATA;
    event->type = DOTS_EVENT_RUN;
    event->offset = prv->run.offset;
    event->run = prv->run.head;
    break;
  case DOTS__STATE_FOURCC:
  case DOTS__STATE_FRAME:
  case DOTS__STATE_BLOCKS:
  case DOTS__STATE_TEXT:
  case DOTS__STATE_DATA:
  case DOTS__STATE_END:
  case DOTS__STATE_ERROR:
    break; /* not reached: no field */
  }
  return 1;
}

/**
 * Reads the start of a frame, or end-of-stream. A token other than a
 * clear is left to the block layer.
 *
 * @param prv the reader's state
 * @param octet the next octet of the input
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_frame(struct dots__reader_prv *prv,
                               unsigned char octet, struct dots_event *event) {
  if (octet == DOTS__END_OF_STREAM) {
    return dots__token(prv, DOTS_EVENT_END_OF_STREAM, DOTS__STATE_END, event);
  }
  if (octet == DOTS__CLEAR) {
    event->clear = 1;
    return dots__token(prv, DOTS_EVENT_FRAME, DOTS__STATE_BLOCKS, event);
  }
  if (octet != DOTS__START_OF_RUN && octet != DOTS__END_OF_BLOCKS) {
    return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  event->type = DOTS_EVENT_FRAME;
  event->offset = prv->offset;
  prv->state = DOTS__STATE_BLOCKS;
  return 0;
}

/**
 * Reads the start of a run, or the end of the layer.
 *
 * @param prv the reader's state
 * @param octet the next octet of the input
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_layer(struct dots__reader_prv *prv,
                               unsigned char octet, struct dots_event *event) {
  int text = prv->state == DOTS__STATE_TEXT;

  if (octet == DOTS__START_OF_RUN) {
    if (text && !(prv->header.flags & 1)) {
      return dots__fail(prv, DOTS_UNEXPECTED_TEXT, prv->offset, event);
    }
    prv->run.head.text = text;
    prv->run.offset = prv->offset;
    prv->state = DOTS__STATE_X;
    ++prv->offset;
    return 1;
  }
  if (!text && octet == DOTS__END_OF_BLOCKS) {
    return dots__token(prv, DOTS_EVENT_END_OF_BLOCKS, DOTS__STATE_TEXT, event);
  }
  if (text && octet == DOTS__END_OF_TEXT) {
    return dots__token(prv, DOTS_EVENT_END_OF_TEXT, DOTS__STATE_FRAME, event);
  }
  return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
}

/**
 * Reads blocks of the current run, as much as the input and the row
 * hold (RUN-1, DEC-1, DEC-5).
 *
 * @param prv the reader's state
 * @param data the rest of the input
 * @param size the number of octets at data, at least 1
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_blocks(struct dots__reader_prv *prv,
                                const unsigned char *data, size_t size,
                                struct dots_event *event) {
  uint64_t limit = prv->run.data_left < prv->run.cells_left
                       ? prv->run.data_left
                       : prv->run.cells_left;
  size_t used = limit < size ? (size_t)limit : size;

  if (limit == 0) {
    return dots__fail(prv, DOTS_OUTSIDE_GRID, prv->offset, event);
  }
  event->type = DOTS_EVENT_CHUNK;
  event->offset = prv->offset;
  event->chunk.octets = data;
  event->chunk.size = used;
  prv->run.data_left -= used;
  prv->run.cells_left -= used;
  prv->offset += used;
  prv->state = prv->run.data_left == 0 ? dots__layer(prv) : DOTS__STATE_DATA;
  return used;
}

/**
 * The length of a UTF-8 sequence, from its first octet (Section 4 of
 * RFC 3629).
 *
 * @param lead the first octet
 * @return the length, from 1 to 4, or 0 if no sequence starts with lead
 */
static unsigned dots__utf8_length(unsigned char lead) {
  if (lead <= 0x7F) {
    return 1;
  }
  if (lead >= 0xC2 && lead <= 0xDF) {
    return 2;
  }
  if (lead >= 0xE0 && lead <= 0xEF) {
    return 3;
  }
  if (lead >= 0xF0 && lead <= 0xF4) {
    return 4;
  }
  return 0;
}

/**
 * The character of a well-formed UTF-8 sequence.
 *
 * @param octets the sequence
 * @param length its length, from 1 to 4, as dots__utf8_length gives it
 * @return its Unicode scalar value
 */
static uint32_t dots__utf8_decode(const unsigned char *octets,
                                  unsigned length) {
  static const unsigned char lead_bits[4] = {0x7F, 0x1F, 0x0F, 0x07};
  uint32_t character = octets[0] & lead_bits[length - 1];
  unsigned i;

  for (i = 1; i < length; ++i) {
    character = character << 6 | (octets[i] & 0x3Fu);
  }
  return character;
}

/**
 * Adds one octet to a UTF-8 sequence, well-formed as in Section 4 of
 * RFC 3629 (DEC-3). After a complete sequence, the octet starts a new
 * one.
 *
 * @param utf8 the sequence
 * @param octet the octet
 * @param offset the offset of the octet
 * @return what the octet does to the sequence
 */
static enum dots__utf8_result
dots__utf8_push(struct dots__utf8 *utf8, unsigned char octet, uint64_t offset) {
  unsigned char low = 0x80;
  unsigned char high = 0xBF;

  if (utf8->size == utf8->length) {
    utf8->size = 0;
    utf8->length = dots__utf8_length(octet);
    utf8->offset = offset;
    if (utf8->length == 0) {
      return DOTS__UTF8_INVALID;
    }
  } else {
    if (utf8->size == 1) {
      switch (utf8->octets[0]) {
      case 0xE0:
        low = 0xA0;
        break;
      case 0xED:
        high = 0x9F;
        break;
      case 0xF0:
        low = 0x90;
        break;
      case 0xF4:
        high = 0x8F;
        break;
      default:
        break;
      }
    }
    if (octet < low || octet > high) {
      return DOTS__UTF8_INVALID;
    }
  }
  utf8->octets[utf8->size] = octet;
  ++utf8->size;
  return utf8->size < utf8->length ? DOTS__UTF8_MORE : DOTS__UTF8_DONE;
}

/**
 * Reads characters of the current run, as much as the input and the
 * row hold (RUN-1, DEC-2, DEC-3, DEC-5). Gives a chunk of the whole
 * sequences in the input, or the one sequence begun in an earlier call,
 * which is in the reader. A sequence that ends past the input is kept
 * in the reader. Before an error, whole sequences are given first.
 *
 * @param prv the reader's state
 * @param data the rest of the input
 * @param size the number of octets at data, at least 1
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t dots__read_text(struct dots__reader_prv *prv,
                              const unsigned char *data, size_t size,
                              struct dots_event *event) {
  size_t end = prv->run.data_left < size ? (size_t)prv->run.data_left : size;
  int carried = prv->utf8.size < prv->utf8.length;
  uint64_t start = prv->offset;
  size_t used = 0;
  size_t whole = 0;
  enum dots_code code = DOTS_OK;

  while (used < end && code == DOTS_OK) {
    enum dots__utf8_result result =
        dots__utf8_push(&prv->utf8, data[used], prv->offset);

    if (result == DOTS__UTF8_INVALID) {
      code = DOTS_INVALID_TEXT;
    } else if (result == DOTS__UTF8_DONE && prv->run.cells_left == 0) {
      code = DOTS_OUTSIDE_GRID;
    } else {
      ++used;
      ++prv->offset;
      --prv->run.data_left;
      if (result == DOTS__UTF8_DONE) {
        --prv->run.cells_left;
        whole = used;
        if (carried) {
          break;
        }
      }
    }
  }
  if (code == DOTS_OK && prv->run.data_left == 0 &&
      prv->utf8.size < prv->utf8.length) {
    code = DOTS_INVALID_TEXT; /* the run ends inside a sequence */
  }
  if (code != DOTS_OK && whole == 0) {
    return dots__fail(prv, code, prv->utf8.offset, event);
  }
  if (code != DOTS_OK) { /* the next call reads the rest again */
    prv->offset = start + whole;
    prv->run.data_left += used - whole;
    prv->utf8.size = 0;
    prv->utf8.length = 0;
    used = whole;
  }
  prv->state = prv->run.data_left == 0 ? dots__layer(prv) : DOTS__STATE_DATA;
  if (whole == 0) {
    return used; /* no event: a sequence goes on past the input */
  }
  event->type = DOTS_EVENT_CHUNK;
  if (carried) {
    event->offset = prv->utf8.offset;
    event->chunk.octets = prv->utf8.octets;
    event->chunk.size = prv->utf8.size;
  } else {
    event->offset = start;
    event->chunk.octets = data;
    event->chunk.size = whole;
  }
  return used;
}

void dots_reader_init(struct dots_reader *reader) {
  static const struct dots_reader initial = DOTS__ZERO;

  *reader = initial;
}

size_t dots_reader_feed(struct dots_reader *reader, const unsigned char *data,
                        size_t size, struct dots_event *event) {
  static const struct dots_event none = DOTS__ZERO;
  struct dots__reader_prv *prv = &reader->prv;
  size_t used = 0;

  if (prv->state == DOTS__STATE_ERROR) {
    *event = prv->error;
    return 0;
  }
  *event = none;
  if (prv->state == DOTS__STATE_END) {
    return size; /* STR-2 */
  }
  while (used < size && event->type == DOTS_EVENT_NONE) {
    unsigned char octet = data[used];

    switch (prv->state) {
    case DOTS__STATE_FOURCC:
      used += dots__read_fourcc(prv, octet, event);
      break;
    case DOTS__STATE_MAJOR:
    case DOTS__STATE_MINOR:
    case DOTS__STATE_UPDATE:
    case DOTS__STATE_FLAGS:
    case DOTS__STATE_COLUMNS:
    case DOTS__STATE_ROWS:
    case DOTS__STATE_FRAME_RATE:
    case DOTS__STATE_X:
    case DOTS__STATE_Y:
    case DOTS__STATE_COUNT:
      used += dots__read_field(prv, octet, event);
      break;
    case DOTS__STATE_FRAME:
      used += dots__read_frame(prv, octet, event);
      break;
    case DOTS__STATE_BLOCKS:
    case DOTS__STATE_TEXT:
      used += dots__read_layer(prv, octet, event);
      break;
    case DOTS__STATE_DATA:
      if (prv->run.head.text) {
        used += dots__read_text(prv, data + used, size - used, event);
      } else {
        used += dots__read_blocks(prv, data + used, size - used, event);
      }
      break;
    case DOTS__STATE_END:
    case DOTS__STATE_ERROR:
      break; /* not reached: an event ends the loop */
    }
  }
  return used;
}

void dots_reader_finish(struct dots_reader *reader, struct dots_event *event) {
  static const struct dots_event none = DOTS__ZERO;
  struct dots__reader_prv *prv = &reader->prv;

  if (prv->state == DOTS__STATE_ERROR) {
    *event = prv->error;
    return;
  }
  *event = none;
  if (prv->state == DOTS__STATE_END) {
    event->type = DOTS_EVENT_END_OF_STREAM;
    event->offset = prv->offset - 1; /* the end-of-stream token */
    return;
  }
  event->type = DOTS_EVENT_ERROR;
  event->code = DOTS_TRUNCATED;
  event->offset = prv->offset;
}

/**
 * Writes a leb128 (INT-1), with no 0x00 tail.
 *
 * @param value its value
 * @param[out] out where its octets go: room for 10
 * @return the number of octets written, 1 to 10
 */
static size_t dots__leb128_write(uint64_t value, unsigned char *out) {
  size_t size = 0;

  while (value > 0x7F) {
    out[size] = (unsigned char)((value & 0x7F) | 0x80);
    ++size;
    value >>= 7;
  }
  out[size] = (unsigned char)value;
  return size + 1;
}

/**
 * Encodes an event other than a chunk.
 *
 * @param event the event
 * @param[out] out where its octets go: room for DOTS_WRITER_MAX
 * @return the number of octets
 */
static size_t dots__encode(const struct dots_event *event, unsigned char *out) {
  size_t size = 0;
  size_t i;

  switch (event->type) {
  case DOTS_EVENT_HEADER:
    for (i = 0; i < sizeof dots__fourcc; ++i) {
      out[i] = dots__fourcc[i];
    }
    size = sizeof dots__fourcc;
    size += dots__leb128_write(event->header.major, out + size);
    size += dots__leb128_write(event->header.minor, out + size);
    size += dots__leb128_write(event->header.update, out + size);
    size += dots__leb128_write(event->header.flags, out + size);
    size += dots__leb128_write(event->header.columns, out + size);
    size += dots__leb128_write(event->header.rows, out + size);
    size += dots__leb128_write(event->header.frame_rate, out + size);
    break;
  case DOTS_EVENT_FRAME:
    if (event->clear) {
      out[0] = DOTS__CLEAR;
      size = 1;
    }
    break;
  case DOTS_EVENT_RUN:
    out[0] = DOTS__START_OF_RUN;
    size = 1;
    size += dots__leb128_write(event->run.x, out + size);
    size += dots__leb128_write(event->run.y, out + size);
    size += dots__leb128_write(event->run.count, out + size);
    break;
  case DOTS_EVENT_END_OF_BLOCKS:
    out[0] = DOTS__END_OF_BLOCKS;
    size = 1;
    break;
  case DOTS_EVENT_END_OF_TEXT:
    out[0] = DOTS__END_OF_TEXT;
    size = 1;
    break;
  case DOTS_EVENT_END_OF_STREAM:
    out[0] = DOTS__END_OF_STREAM;
    size = 1;
    break;
  case DOTS_EVENT_NONE:
  case DOTS_EVENT_CHUNK:
  case DOTS_EVENT_ERROR:
    break; /* not reached: no such event is encoded */
  }
  return size;
}

/**
 * Whether the stream written so far may go on with an event of the
 * type of the event, in the layer it names.
 *
 * @param prv the writer's state
 * @param event the event
 * @return 1 if it may, otherwise 0
 */
static int dots__writer_allows(const struct dots__writer_prv *prv,
                               const struct dots_event *event) {
  enum dots__state state =
      prv->frame ? DOTS__STATE_BLOCKS : prv->reader.prv.state;

  switch (event->type) {
  case DOTS_EVENT_HEADER:
    return state == DOTS__STATE_FOURCC;
  case DOTS_EVENT_FRAME:
  case DOTS_EVENT_END_OF_STREAM:
    return state == DOTS__STATE_FRAME;
  case DOTS_EVENT_RUN:
    return event->run.text ? state == DOTS__STATE_TEXT
                           : state == DOTS__STATE_BLOCKS;
  case DOTS_EVENT_CHUNK:
    return state == DOTS__STATE_DATA;
  case DOTS_EVENT_END_OF_BLOCKS:
    return state == DOTS__STATE_BLOCKS;
  case DOTS_EVENT_END_OF_TEXT:
    return state == DOTS__STATE_TEXT;
  case DOTS_EVENT_NONE:
  case DOTS_EVENT_ERROR:
    return 0;
  }
  return 0;
}

/**
 * Feeds octets to a reader, all of them unless it gives an error.
 *
 * @param reader the reader
 * @param octets the octets
 * @param size the number of octets
 * @return DOTS_OK, or the code of the error
 */
static enum dots_code dots__check(struct dots_reader *reader,
                                  const unsigned char *octets, size_t size) {
  struct dots_event event;
  size_t used = 0;

  while (used < size) {
    used += dots_reader_feed(reader, octets + used, size - used, &event);
    if (event.type == DOTS_EVENT_ERROR) {
      return event.code;
    }
  }
  return DOTS_OK;
}

/**
 * Whether the current run can still end well-formed: its data left
 * fits the cells left in its row (DEC-5), and in the text layer
 * completes the sequence in progress (DEC-3). A character takes at
 * most 4 octets.
 *
 * @param prv the reader's state
 * @return DOTS_OK, or the code of the requirement no data can satisfy
 */
static enum dots_code dots__completable(const struct dots__reader_prv *prv) {
  uint64_t data = prv->run.data_left;
  uint64_t cells = prv->run.cells_left;

  if (prv->state != DOTS__STATE_DATA) {
    return DOTS_OK; /* no run, or it has ended */
  }
  if (!prv->run.head.text) {
    return data <= cells ? DOTS_OK : DOTS_OUTSIDE_GRID;
  }
  if (prv->utf8.size < prv->utf8.length) {
    unsigned need = prv->utf8.length - prv->utf8.size;

    if (data < need) {
      return DOTS_INVALID_TEXT;
    }
    if (cells == 0) {
      return DOTS_OUTSIDE_GRID;
    }
    data -= need;
    --cells;
  }
  return data / 4 + (data % 4 != 0) <= cells ? DOTS_OK : DOTS_OUTSIDE_GRID;
}

void dots_writer_init(struct dots_writer *writer) {
  static const struct dots_writer initial = DOTS__ZERO;

  *writer = initial;
}

struct dots_writer_result dots_writer_put(struct dots_writer *writer,
                                          const struct dots_event *event,
                                          unsigned char *out, size_t capacity) {
  static const struct dots_writer_result none = DOTS__ZERO;
  struct dots__writer_prv *prv = &writer->prv;
  struct dots_writer_result result = none;
  unsigned char encoded[DOTS_WRITER_MAX];
  const unsigned char *octets = encoded;
  struct dots_reader reader;
  size_t size;
  size_t i;

  if (!dots__writer_allows(prv, event)) {
    result.status = DOTS_WRITER_OUT_OF_ORDER;
    return result;
  }
  if (event->type == DOTS_EVENT_CHUNK) {
    if (event->chunk.size > prv->reader.prv.run.data_left) {
      result.status = DOTS_WRITER_PAST_COUNT;
      return result;
    }
    octets = event->chunk.octets;
    size = event->chunk.size;
  } else {
    size = dots__encode(event, encoded);
  }
  if (size > capacity) {
    result.status = DOTS_WRITER_NO_SPACE;
    return result;
  }
  reader = prv->reader;
  result.code = dots__check(&reader, octets, size);
  if (result.code == DOTS_OK) {
    result.code = dots__completable(&reader.prv);
  }
  if (result.code != DOTS_OK) {
    result.status = DOTS_WRITER_ILL_FORMED;
    return result;
  }
  prv->reader = reader;
  prv->frame = event->type == DOTS_EVENT_FRAME && !event->clear;
  for (i = 0; i < size; ++i) {
    out[i] = octets[i];
  }
  result.size = size;
  return result;
}

/**
 * Gives every cell the initial state: the block 0x00 and no character.
 *
 * @param prv the composer's state
 */
static void dots__clear(struct dots__composer_prv *prv) {
  static const struct dots_cell initial = DOTS__ZERO;
  size_t i;

  for (i = 0; i < prv->columns * prv->rows; ++i) {
    prv->cells[i] = initial;
  }
}

/**
 * Sets the blocks of a chunk (CMP-2).
 *
 * @param prv the composer's state
 * @param chunk the chunk
 */
static void dots__compose_blocks(struct dots__composer_prv *prv,
                                 const struct dots_chunk *chunk) {
  size_t i;

  for (i = 0; i < chunk->size && prv->next < prv->columns * prv->rows; ++i) {
    prv->cells[prv->next].block = chunk->octets[i];
    ++prv->next;
  }
}

/**
 * Sets the characters of a chunk, whole well-formed UTF-8 sequences
 * (CMP-3). The eraser is 0, so setting it removes the character.
 *
 * @param prv the composer's state
 * @param chunk the chunk
 */
static void dots__compose_text(struct dots__composer_prv *prv,
                               const struct dots_chunk *chunk) {
  size_t i = 0;

  while (i < chunk->size && prv->next < prv->columns * prv->rows) {
    unsigned length = dots__utf8_length(chunk->octets[i]);

    if (length == 0 || length > chunk->size - i) {
      return; /* not a chunk of the reader */
    }
    prv->cells[prv->next].character =
        dots__utf8_decode(chunk->octets + i, length);
    ++prv->next;
    i += length;
  }
}

int dots_composer_init(struct dots_composer *composer,
                       const struct dots_header *header,
                       struct dots_cell *cells, size_t count) {
  struct dots__composer_prv *prv = &composer->prv;

  if (header->rows != 0 && header->columns > count / header->rows) {
    return 0;
  }
  prv->cells = cells;
  prv->columns = (size_t)header->columns;
  prv->rows = (size_t)header->rows;
  prv->next = 0;
  prv->text = 0;
  dots__clear(prv);
  return 1;
}

void dots_composer_apply(struct dots_composer *composer,
                         const struct dots_event *event) {
  struct dots__composer_prv *prv = &composer->prv;

  switch (event->type) {
  case DOTS_EVENT_FRAME:
    if (event->clear) {
      dots__clear(prv);
    }
    break;
  case DOTS_EVENT_RUN:
    prv->text = event->run.text;
    if (event->run.x < prv->columns && event->run.y < prv->rows) {
      prv->next = (size_t)event->run.y * prv->columns + (size_t)event->run.x;
    } else {
      prv->next = prv->columns * prv->rows; /* nothing to set */
    }
    break;
  case DOTS_EVENT_CHUNK:
    if (prv->text) {
      dots__compose_text(prv, &event->chunk);
    } else {
      dots__compose_blocks(prv, &event->chunk);
    }
    break;
  case DOTS_EVENT_NONE:
  case DOTS_EVENT_HEADER:
  case DOTS_EVENT_END_OF_BLOCKS:
  case DOTS_EVENT_END_OF_TEXT:
  case DOTS_EVENT_END_OF_STREAM:
  case DOTS_EVENT_ERROR:
    break;
  }
}

uint32_t dots_braille(unsigned char block) {
  static const unsigned char values[8] = {0x01, 0x08, 0x02, 0x10,
                                          0x04, 0x20, 0x40, 0x80};
  uint32_t pattern = 0x2800;
  unsigned n;

  for (n = 0; n < 8; ++n) {
    if (block & 1u << n) {
      pattern += values[n];
    }
  }
  return pattern;
}

#endif /* DOTS_IMPLEMENTATION */
