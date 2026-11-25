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
struct dots__leb128 {
  uint64_t value;  /**< its value, so far */
  uint64_t offset; /**< its offset */
  unsigned shift;  /**< the shift of its next octet; 0 before its first */
  int overflow;    /**< its value is above 2^64 - 1 */
};

/** What an octet does to a leb128. */
enum dots__leb128_result {
  DOTS__LEB128_MORE,      /**< the leb128 continues */
  DOTS__LEB128_DONE,      /**< the leb128 ends: its value is in value */
  DOTS__LEB128_MALFORMED, /**< the octet is a 0x00 tail */
  DOTS__LEB128_OVERFLOW   /**< the leb128 ends, above 2^64 - 1 */
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
  struct dots__leb128 leb128;
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
static enum dots__leb128_result dots__leb128_push(struct dots__leb128 *leb128,
                                                  unsigned char octet,
                                                  uint64_t offset) {
  uint64_t bits = octet & 0x7F;
  enum dots__leb128_result result;

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
    return DOTS__LEB128_MORE;
  }
  if (octet == 0 && leb128->shift != 0) {
    return DOTS__LEB128_MALFORMED;
  }
  result = leb128->overflow ? DOTS__LEB128_OVERFLOW : DOTS__LEB128_DONE;
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
  static const unsigned char fourcc[4] = {0x64, 0x6F, 0x74, 0x73};

  if (octet != fourcc[prv->offset]) {
    return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  prv->state =
      prv->offset + 1 == sizeof fourcc ? DOTS__STATE_MAJOR : DOTS__STATE_FOURCC;
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
  enum dots__leb128_result result =
      dots__leb128_push(&prv->leb128, octet, prv->offset);
  uint64_t value = prv->leb128.value;

  if (result == DOTS__LEB128_MALFORMED) {
    return dots__fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  if (result == DOTS__LEB128_OVERFLOW) {
    return dots__fail(prv, DOTS_OVERFLOW, prv->leb128.offset, event);
  }
  ++prv->offset;
  if (result == DOTS__LEB128_MORE) {
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
