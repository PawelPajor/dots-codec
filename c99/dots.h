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
 * The interface has two levels. The event level, `dots_evt_`, gives
 * and takes a stream event by event and allocates nothing. The model
 * level, `dots_mdl_`, is built on it: it gives and takes whole frames,
 * in memory from the caller. Names both levels share start with
 * `dots_`. Macros and constants are in capitals: `DOTS_EVT_`,
 * `DOTS_MDL_`, `DOTS_`.
 *
 * Names of the implementation start with `dots_detail_evt_`,
 * `dots_detail_mdl_`, or `dots_detail_` if both levels share them, and
 * in capitals likewise.
 * They are not part of the interface, and may change at any time.
 *
 * The private state of a struct `dots_evt_x` is its member `prv`, of
 * type `struct dots_detail_evt_x_prv`, and likewise for `dots_mdl_x`.
 */

/**
 * @mainpage DOTS in C99
 *
 * @htmlonly
 * <picture>
 *   <source media="(prefers-color-scheme: dark)"
 *           srcset="dots-braille-dark.png">
 *   <img alt="DOTS" src="dots-braille-light.png" style="max-width: 100%">
 * </picture>
 * @endhtmlonly
 *
 * A reader and a writer of
 * [DOTS](https://github.com/PawelPajor/dots-stream) streams in one
 * header, dots.h: no dependencies. See dots.h for how to include it.
 *
 * The @ref model "model level" reads and writes whole frames, with
 * memory from the caller. The event level reads and writes a stream
 * @ref events "event by event" and allocates nothing: a
 * @ref reader "reader", a @ref writer "writer", and a
 * @ref composer "composer" that keeps the state of the grid.
 *
 * Examples:
 *
 * - @ref model_read.c reads a stream and prints each frame.
 * - @ref model_write.c writes a stream of three frames.
 * - @ref event_read.c is model_read.c at the event level.
 * - @ref event_write.c is model_write.c at the event level: the same octets.
 */
/**
 * @example model_read.c
 * Reads a stream at the model level and prints each frame.
 */
/**
 * @example model_write.c
 * Writes a stream of three frames at the model level.
 */
/**
 * @example event_read.c
 * Reads a stream at the event level and prints each frame.
 */
/**
 * @example event_write.c
 * Writes a stream of three frames at the event level.
 */

#ifndef DOTS_H
#define DOTS_H

#include <stdbool.h>
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
 * @return its name, or NULL if the code is unknown
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
enum dots_evt_event_type {
  /** no event */
  DOTS_EVT_EVENT_NONE,
  /** the header: event.header */
  DOTS_EVT_EVENT_HEADER,
  /** the start of a frame: event.clear */
  DOTS_EVT_EVENT_FRAME,
  /** the start of a run: event.run */
  DOTS_EVT_EVENT_RUN,
  /** octets of the current run: event.chunk */
  DOTS_EVT_EVENT_CHUNK,
  /** the end of the block layer */
  DOTS_EVT_EVENT_END_OF_BLOCKS,
  /** the end of the text layer and the frame */
  DOTS_EVT_EVENT_END_OF_TEXT,
  /** the end of the stream */
  DOTS_EVT_EVENT_END_OF_STREAM,
  /** an error: event.code */
  DOTS_EVT_EVENT_ERROR
};

/** A version (Section 4.3). */
struct dots_version {
  uint64_t major;
  uint64_t minor;
  uint64_t update;
};

/** 2D coordinates (Section 4.4). */
struct dots_coordinates {
  uint64_t x;
  uint64_t y;
};

/** The header of a stream (Section 5). */
struct dots_header {
  /**
   * dots-version. Not named dots_version: in C++, a member named after
   * its type changes the meaning of that name in the struct.
   */
  struct dots_version version;
  uint64_t flags;
  struct dots_coordinates dimensions; /**< columns, rows */
  uint64_t frame_rate;
};

/** The start of a run. */
struct dots_evt_run {
  bool text; /**< in the text layer, else in the block layer */
  uint64_t x;
  uint64_t y;
  uint64_t count;
};

/** A piece of the data of the current run. */
struct dots_evt_chunk {
  const unsigned char *octets;
  size_t size;
};

/** An event. Its type says which members hold it. */
struct dots_evt_event {
  enum dots_evt_event_type type;
  uint64_t offset; /**< the offset of the event, or of the error */
  enum dots_code code;
  bool clear; /**< the frame starts with a clear */
  struct dots_header header;
  struct dots_evt_run run;
  struct dots_evt_chunk chunk;
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
 * struct dots_evt_reader reader;
 * struct dots_evt_event event;
 *
 * dots_evt_reader_init(&reader);
 * while (size > 0) {
 *   size_t used = dots_evt_reader_feed(&reader, data, size, &event);
 *
 *   data += used;
 *   size -= used;
 *   ... handle event, stop on END_OF_STREAM or ERROR ...
 * }
 * dots_evt_reader_finish(&reader, &event);   at the end of input
 * @endcode
 *
 * @{
 */

/** The states of a reader: the part of the stream it reads next. */
enum dots_detail_evt_state {
  DOTS_DETAIL_EVT_STATE_FOURCC,
  DOTS_DETAIL_EVT_STATE_MAJOR,
  DOTS_DETAIL_EVT_STATE_MINOR,
  DOTS_DETAIL_EVT_STATE_UPDATE,
  DOTS_DETAIL_EVT_STATE_FLAGS,
  DOTS_DETAIL_EVT_STATE_COLUMNS,
  DOTS_DETAIL_EVT_STATE_ROWS,
  DOTS_DETAIL_EVT_STATE_FRAME_RATE,

  /** a frame, or end-of-stream */
  DOTS_DETAIL_EVT_STATE_FRAME,

  /** a run, or end-of-blocks */
  DOTS_DETAIL_EVT_STATE_BLOCKS,

  /** a run, or end-of-text */
  DOTS_DETAIL_EVT_STATE_TEXT,

  DOTS_DETAIL_EVT_STATE_X,
  DOTS_DETAIL_EVT_STATE_Y,
  DOTS_DETAIL_EVT_STATE_COUNT,
  DOTS_DETAIL_EVT_STATE_DATA,

  /** nothing: the stream has ended */
  DOTS_DETAIL_EVT_STATE_END,

  DOTS_DETAIL_EVT_STATE_ERROR
};

/** The run being read. */
struct dots_detail_evt_run {
  struct dots_evt_run head; /**< its layer, position and count */
  uint64_t offset;          /**< its offset */
  uint64_t data_left;       /**< its data octets not yet read */
  uint64_t cells_left;      /**< the cells left in its row */
};

/** A leb128, read one octet at a time. */
struct dots_detail_evt_leb128_reader {
  uint64_t value;  /**< its value, so far */
  uint64_t offset; /**< its offset */
  unsigned shift;  /**< the shift of its next octet; 0 before its first */
  bool overflow;   /**< its value is above 2^64 - 1 */
};

/** What an octet does to a leb128. */
enum dots_detail_evt_leb128_reader_result {
  /** the leb128 continues */
  DOTS_DETAIL_EVT_LEB128_READER_MORE,

  /** the leb128 ends: its value is in value */
  DOTS_DETAIL_EVT_LEB128_READER_DONE,

  /** the octet is a 0x00 tail */
  DOTS_DETAIL_EVT_LEB128_READER_MALFORMED,

  /** the leb128 ends, above 2^64 - 1 */
  DOTS_DETAIL_EVT_LEB128_READER_OVERFLOW
};

/** The most octets of a UTF-8 sequence (Section 3 of RFC 3629). */
#define DOTS_DETAIL_UTF8_MAX 4

/**
 * A UTF-8 sequence, read one octet at a time. It is in progress while
 * size < length, and complete when size == length.
 */
struct dots_detail_evt_utf8 {
  unsigned char octets[DOTS_DETAIL_UTF8_MAX]; /**< its octets read */
  unsigned size;                              /**< their number */
  unsigned length; /**< its length, set by its first octet */
  uint64_t offset; /**< its offset */
};

/** What an octet does to a UTF-8 sequence. */
enum dots_detail_evt_utf8_result {
  /** the sequence continues */
  DOTS_DETAIL_EVT_UTF8_MORE,
  /** the sequence ends: it is in octets */
  DOTS_DETAIL_EVT_UTF8_DONE,
  /** the sequence is ill-formed */
  DOTS_DETAIL_EVT_UTF8_INVALID
};

/** The state of a reader: private, it may change at any time. */
struct dots_detail_evt_reader_prv {
  enum dots_detail_evt_state state;
  uint64_t offset;
  struct dots_header header;
  struct dots_detail_evt_run run;
  struct dots_detail_evt_leb128_reader leb128;
  struct dots_detail_evt_utf8 utf8;
  struct dots_evt_event error;
};

/** A reader. */
struct dots_evt_reader {
  struct dots_detail_evt_reader_prv prv; /**< private */
};

/**
 * Starts reading a stream.
 *
 * @param reader the reader
 */
void dots_evt_reader_init(struct dots_evt_reader *reader);

/**
 * Reads the input up to the next event. DOTS_EVT_EVENT_NONE means all
 * input is used and more is needed. A chunk points into the input or
 * into the reader, and is valid until the next call; in the text layer
 * it holds only whole, well-formed UTF-8 sequences. After
 * DOTS_EVT_EVENT_END_OF_STREAM, which means the stream is well-formed, it
 * uses all input and ignores it (STR-2). After DOTS_EVT_EVENT_ERROR it uses
 * nothing and gives the same error.
 *
 * @param reader the reader
 * @param data the next octets of the stream
 * @param size the number of octets at data
 * @param[out] event the event
 * @return the number of octets used
 */
size_t dots_evt_reader_feed(struct dots_evt_reader *reader,
                            const unsigned char *data, size_t size,
                            struct dots_evt_event *event);

/**
 * Ends the input. Gives DOTS_EVT_EVENT_END_OF_STREAM if the stream ended,
 * otherwise the error: TRUNCATED at the end of the stream, or the
 * error already found.
 *
 * @param reader the reader
 * @param[out] event the event
 */
void dots_evt_reader_finish(struct dots_evt_reader *reader,
                            struct dots_evt_event *event);

/** @} */

/**
 * @defgroup writer Writer
 *
 * Encodes events into a stream, one call per event, in a buffer the
 * caller provides. A chunk is checked, not copied: its octets are its
 * own. It refuses any event that would make the stream ill-formed
 * (CNF-1), and then writes nothing and keeps its state. The writer
 * allocates nothing and never holds the stream.
 *
 * @code
 * struct dots_evt_writer writer;
 * struct dots_evt_writer_result result;
 *
 * dots_evt_writer_init(&writer);
 * for each event, up to END_OF_STREAM:
 *   result = dots_evt_writer_put(&writer, &event, out, sizeof out);
 *   ... on DOTS_EVT_WRITER_OK, the next result.size octets of the stream
 *       are at result.octets ...
 * @endcode
 *
 * @{
 */

/** The most octets an event other than a chunk is written as. */
#define DOTS_EVT_WRITER_MAX 74

/** What the writer did with an event. */
enum dots_evt_writer_status {
  /** the event is written */
  DOTS_EVT_WRITER_OK,
  /** it breaks a stream requirement: code */
  DOTS_EVT_WRITER_ILL_FORMED,
  /** the stream has no such event next */
  DOTS_EVT_WRITER_OUT_OF_ORDER,
  /** a chunk past the count of its run */
  DOTS_EVT_WRITER_PAST_COUNT,
  /** an event but a chunk whose octets do not fit the capacity */
  DOTS_EVT_WRITER_NO_SPACE
};

/** The result of a call of the writer. */
struct dots_evt_writer_result {
  enum dots_evt_writer_status status;
  enum dots_code code; /**< the code of the requirement, if ILL_FORMED */
  /** the event's octets: in out, or a chunk's own; NULL if refused */
  const unsigned char *octets;
  size_t size; /**< the number of octets at octets */
};

/** The state of a writer: private, it may change at any time. */
struct dots_detail_evt_writer_prv {
  struct dots_evt_reader reader; /**< has read every octet written */
  bool frame; /**< a frame without a clear has started, none of it written */
};

/** A writer. */
struct dots_evt_writer {
  struct dots_detail_evt_writer_prv prv; /**< private */
};

/**
 * Starts writing a stream.
 *
 * @param writer the writer
 */
void dots_evt_writer_init(struct dots_evt_writer *writer);

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
 * @param[out] out where the octets of any event but a chunk go
 * @param capacity the number of octets out holds: DOTS_EVT_WRITER_MAX is
 *        enough for any event; a chunk uses none
 * @return DOTS_EVT_WRITER_OK and where the event's octets are, or a
 *         refusal
 */
struct dots_evt_writer_result
dots_evt_writer_put(struct dots_evt_writer *writer,
                    const struct dots_evt_event *event, unsigned char *out,
                    size_t capacity);

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
 * struct dots_evt_composer composer;
 *
 * on DOTS_EVT_EVENT_HEADER:
 *   if (!dots_evt_composer_init(&composer, &event.header, cells, count)) {
 *     ... the grid has more cells than count ...
 *   }
 * on any other event:
 *   dots_evt_composer_apply(&composer, &event);
 * on DOTS_EVT_EVENT_END_OF_TEXT:
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
struct dots_detail_evt_composer_prv {
  struct dots_cell *cells;
  size_t columns;
  size_t rows;
  size_t next; /**< the index of the next cell of the run */
  bool text;   /**< the run is in the text layer */
};

/** A composer. */
struct dots_evt_composer {
  struct dots_detail_evt_composer_prv prv; /**< private */
};

/**
 * Starts composing a stream, from the initial state.
 *
 * @param composer the composer
 * @param header the header of the stream
 * @param cells the cells of the state
 * @param count the number of cells at cells
 * @return true, or false if count is less than columns * rows
 */
bool dots_evt_composer_init(struct dots_evt_composer *composer,
                            const struct dots_header *header,
                            struct dots_cell *cells, size_t count);

/**
 * Applies an event of the reader that read the header given to
 * dots_evt_composer_init. A frame with a clear and a chunk change the
 * state, a run says where its chunks go, other events do nothing.
 *
 * @param composer the composer
 * @param event the event
 */
void dots_evt_composer_apply(struct dots_evt_composer *composer,
                             const struct dots_evt_event *event);

/**
 * The Braille pattern of a block (Section 5 of dots-content.md).
 *
 * @param block a block
 * @return its code point, from U+2800 to U+28FF
 */
uint32_t dots_braille(unsigned char block);

/** @} */

/**
 * @defgroup model Model
 *
 * Whole frames, one C type per ABNF rule, on top of the event level. A
 * source reads a header and then frames from an input, in memory from
 * an allocator. A sink writes a header and then frames to an output.
 * A stream is a header and frames over time: it has no type.
 *
 * @code
 * struct dots_mdl_allocator allocator = {malloc, free};
 * struct dots_mdl_input input = {dots_mdl_stdio_get, in};
 * struct dots_mdl_source source;
 * struct dots_header header;
 * struct dots_mdl_frame *frame;
 *
 * dots_mdl_open_source(&source, allocator, input, &header);
 * while (dots_mdl_get_frame(&source, &frame).status == DOTS_MDL_OK) {
 *   ... show frame ...
 *   dots_mdl_release_frame(&source, frame);
 * }
 * @endcode
 *
 * A frame to write can live anywhere, the stack included:
 *
 * @code
 * unsigned char text[] = "Hi";
 * struct dots_mdl_run hi = {{0, 0}, 2, text, NULL};
 * struct dots_mdl_frame frame = {true, {NULL}, {&hi}}; // clear, no blocks,
 * "Hi"
 *
 * dots_mdl_open_sink(&sink, output, &header);
 * dots_mdl_put_frame(&sink, &frame);
 * dots_mdl_close_sink(&sink); // end-of-stream
 * @endcode
 *
 * Every call but dots_mdl_release_frame and dots_mdl_compose returns a
 * result. Every status but DOTS_MDL_OK is final: each later call of the
 * source or sink gives it again.
 *
 * @{
 */

/** A run: its data, one octet per block or UTF-8 text. */
struct dots_mdl_run {
  struct dots_coordinates position;
  uint64_t count;
  unsigned char *data;       /**< count octets, or NULL if count is 0 */
  struct dots_mdl_run *next; /**< the next run of the layer, or NULL */
};

/** A block layer or a text layer: its end is the end of the list. */
struct dots_mdl_layer {
  struct dots_mdl_run *runs; /**< the first run, or NULL */
};

/** A frame. */
struct dots_mdl_frame {
  bool clear; /**< the frame starts with a clear */
  struct dots_mdl_layer blocks;
  struct dots_mdl_layer text;
};

/**
 * Where a source gets memory: functions with the signatures of malloc
 * and free, which it may be given. Every piece is allocated once and
 * never moves. free is given NULL too.
 */
struct dots_mdl_allocator {
  void *(*allocate)(size_t size);
  void (*free)(void *pointer);
};

/** The input has ended. */
#define DOTS_MDL_IO_END -1

/** The input or output failed. */
#define DOTS_MDL_IO_ERROR -2

/** Where a source gets its octets, one per call. */
struct dots_mdl_input {
  int (*get)(void *handle); /**< 0 to 255, DOTS_MDL_IO_END or _ERROR */
  void *handle;
};

/** Where a sink puts its octets, one per call. */
struct dots_mdl_output {
  int (*put)(int octet, void *handle); /**< 0, or DOTS_MDL_IO_ERROR */
  void *handle;
};

/**
 * fgetc as a dots_mdl_input get. Defined with DOTS_IMPLEMENTATION only
 * if DOTS_STDIO is defined too.
 *
 * @param handle a FILE *
 * @return the octet, DOTS_MDL_IO_END or DOTS_MDL_IO_ERROR
 */
int dots_mdl_stdio_get(void *handle);

/**
 * fputc as a dots_mdl_output put. Defined with DOTS_IMPLEMENTATION only
 * if DOTS_STDIO is defined too.
 *
 * @param octet the octet
 * @param handle a FILE *
 * @return 0, or DOTS_MDL_IO_ERROR
 */
int dots_mdl_stdio_put(int octet, void *handle);

/** What a call of a source or a sink did. */
enum dots_mdl_status {

  /** done: a header or frame given, or written */
  DOTS_MDL_OK,

  /** get_frame: the stream has ended */
  DOTS_MDL_END,

  /** a stream requirement: code */
  DOTS_MDL_ILL_FORMED,

  /** allocate gave NULL, or a count above SIZE_MAX */
  DOTS_MDL_NO_MEMORY,

  /** get or put gave DOTS_MDL_IO_ERROR */
  DOTS_MDL_IO_FAILED,

  /** get gave a value outside its contract */
  DOTS_MDL_BAD_INPUT,

  /** a call after close_sink */
  DOTS_MDL_OUT_OF_ORDER
};

/** The result of a call of a source or a sink. */
struct dots_mdl_result {
  enum dots_mdl_status status;
  enum dots_code code; /**< if ILL_FORMED */
  uint64_t offset;     /**< if ILL_FORMED from a source */
};

/** No octet held: the reader has used the last octet from the input. */
#define DOTS_DETAIL_MDL_NO_OCTET -1

/** The state of a source: private, it may change at any time. */
struct dots_detail_mdl_source_prv {
  struct dots_evt_reader reader;
  struct dots_mdl_allocator allocator;
  struct dots_mdl_input input;
  int octet; /**< got, not yet used by the reader, or DOTS_DETAIL_MDL_NO_OCTET
              */
  struct dots_mdl_result result; /**< OK, or the final result */
};

/** A source: reads frames. */
struct dots_mdl_source {
  struct dots_detail_mdl_source_prv prv; /**< private */
};

/** The state of a sink: private, it may change at any time. */
struct dots_detail_mdl_sink_prv {
  struct dots_evt_writer writer;
  struct dots_mdl_output output;
  struct dots_mdl_result result; /**< OK, or the final result */
};

/** A sink: writes frames. */
struct dots_mdl_sink {
  struct dots_detail_mdl_sink_prv prv; /**< private */
};

/**
 * Starts reading a stream, up to its header. It needs no close.
 *
 * @param source the source
 * @param allocator where its frames get memory
 * @param input where it gets octets
 * @param[out] header the header, on DOTS_MDL_OK
 * @return the result: ILL_FORMED with the reader's code and offset
 */
struct dots_mdl_result dots_mdl_open_source(struct dots_mdl_source *source,
                                            struct dots_mdl_allocator allocator,
                                            struct dots_mdl_input input,
                                            struct dots_header *header);

/**
 * Reads the next frame, with every run. On DOTS_MDL_OK the caller owns
 * the frame and releases it with dots_mdl_release_frame. On any other
 * status there is no frame: the source frees what it had allocated.
 *
 * @param source the source
 * @param[out] frame the frame, or NULL
 * @return the result: END after the end of the stream
 */
struct dots_mdl_result dots_mdl_get_frame(struct dots_mdl_source *source,
                                          struct dots_mdl_frame **frame);

/**
 * Frees a frame that dots_mdl_get_frame gave.
 *
 * @param source the source that gave it
 * @param frame the frame, or NULL
 */
void dots_mdl_release_frame(struct dots_mdl_source *source,
                            struct dots_mdl_frame *frame);

/**
 * Starts writing a stream, with its header.
 *
 * @param sink the sink
 * @param output where it puts octets
 * @param header the header
 * @return the result: ILL_FORMED with the writer's code
 */
struct dots_mdl_result dots_mdl_open_sink(struct dots_mdl_sink *sink,
                                          struct dots_mdl_output output,
                                          const struct dots_header *header);

/**
 * Writes a frame, giving it to the event writer event by event. A
 * refused event may leave the frame partly written.
 *
 * @param sink the sink
 * @param frame the frame
 * @return the result: ILL_FORMED with the writer's code
 */
struct dots_mdl_result dots_mdl_put_frame(struct dots_mdl_sink *sink,
                                          const struct dots_mdl_frame *frame);

/**
 * Ends the stream: writes end-of-stream. Every later call gives
 * DOTS_MDL_OUT_OF_ORDER.
 *
 * @param sink the sink
 * @return the result
 */
struct dots_mdl_result dots_mdl_close_sink(struct dots_mdl_sink *sink);

/**
 * Applies a frame to the state of the grid: replays it, in stream
 * order, to the composer, whose cells the caller keeps across frames.
 *
 * @param composer the composer, started with the header of the stream
 * @param frame a frame of the stream
 */
void dots_mdl_compose(struct dots_evt_composer *composer,
                      const struct dots_mdl_frame *frame);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* DOTS_H */

#if defined(DOTS_IMPLEMENTATION) && !defined(DOTS_IMPLEMENTATION_DONE)
#define DOTS_IMPLEMENTATION_DONE

/** An initializer of a struct with all members zero, in C and C++. */
#ifdef __cplusplus
#define DOTS_DETAIL_ZERO                                                       \
  {                                                                            \
  }
#else
#define DOTS_DETAIL_ZERO {0}
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
  return NULL;
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
static enum dots_detail_evt_leb128_reader_result
dots_detail_evt_leb128_read(struct dots_detail_evt_leb128_reader *leb128,
                            unsigned char octet, uint64_t offset) {
  uint64_t bits = octet & 0x7F;
  enum dots_detail_evt_leb128_reader_result result;

  if (leb128->shift == 0) {
    leb128->offset = offset;
    leb128->value = 0;
    leb128->overflow = false;
  }
  if (leb128->shift < 64) {
    if (leb128->shift == 63 && bits > 1) {
      leb128->overflow = true;
    }
    leb128->value |= bits << leb128->shift;
  } else if (bits != 0) {
    leb128->overflow = true;
  }
  if (octet & 0x80) {
    if (leb128->shift < 64) {
      leb128->shift += 7;
    }
    return DOTS_DETAIL_EVT_LEB128_READER_MORE;
  }
  if (octet == 0 && leb128->shift != 0) {
    return DOTS_DETAIL_EVT_LEB128_READER_MALFORMED;
  }
  result = leb128->overflow ? DOTS_DETAIL_EVT_LEB128_READER_OVERFLOW
                            : DOTS_DETAIL_EVT_LEB128_READER_DONE;
  leb128->shift = 0;
  return result;
}

/** The tokens (Section 4.2). */
enum dots_detail_evt_token {
  DOTS_DETAIL_EVT_CLEAR = 0x01,
  DOTS_DETAIL_EVT_START_OF_RUN = 0x02,
  DOTS_DETAIL_EVT_END_OF_BLOCKS = 0xFE,
  DOTS_DETAIL_EVT_END_OF_TEXT = 0xFD,
  DOTS_DETAIL_EVT_END_OF_STREAM = 0xFF
};

/** The fourcc (Section 5). */
static const unsigned char dots_detail_evt_fourcc[4] = {0x64, 0x6F, 0x74, 0x73};

/**
 * Gives an error, from now on.
 *
 * @param prv the reader's state
 * @param code the code of the error
 * @param offset the offset of the error
 * @param[out] event the error
 * @return 0, the number of octets used
 */
static size_t dots_detail_evt_fail(struct dots_detail_evt_reader_prv *prv,
                                   enum dots_code code, uint64_t offset,
                                   struct dots_evt_event *event) {
  prv->state = DOTS_DETAIL_EVT_STATE_ERROR;
  prv->error.type = DOTS_EVT_EVENT_ERROR;
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
static size_t dots_detail_evt_token(struct dots_detail_evt_reader_prv *prv,
                                    enum dots_evt_event_type type,
                                    enum dots_detail_evt_state next,
                                    struct dots_evt_event *event) {
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
static enum dots_detail_evt_state
dots_detail_evt_layer(const struct dots_detail_evt_reader_prv *prv) {
  return prv->run.head.text ? DOTS_DETAIL_EVT_STATE_TEXT
                            : DOTS_DETAIL_EVT_STATE_BLOCKS;
}

/**
 * Reads an octet of the fourcc.
 *
 * @param prv the reader's state
 * @param octet the next octet of the input
 * @param[out] event the event, if any
 * @return the number of octets used: at least 1, or 0 with an event
 */
static size_t
dots_detail_evt_read_fourcc(struct dots_detail_evt_reader_prv *prv,
                            unsigned char octet, struct dots_evt_event *event) {
  if (octet != dots_detail_evt_fourcc[prv->offset]) {
    return dots_detail_evt_fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  prv->state = prv->offset + 1 == sizeof dots_detail_evt_fourcc
                   ? DOTS_DETAIL_EVT_STATE_MAJOR
                   : DOTS_DETAIL_EVT_STATE_FOURCC;
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
static size_t dots_detail_evt_read_field(struct dots_detail_evt_reader_prv *prv,
                                         unsigned char octet,
                                         struct dots_evt_event *event) {
  enum dots_detail_evt_leb128_reader_result result =
      dots_detail_evt_leb128_read(&prv->leb128, octet, prv->offset);
  uint64_t value = prv->leb128.value;

  if (result == DOTS_DETAIL_EVT_LEB128_READER_MALFORMED) {
    return dots_detail_evt_fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  if (result == DOTS_DETAIL_EVT_LEB128_READER_OVERFLOW) {
    return dots_detail_evt_fail(prv, DOTS_OVERFLOW, prv->leb128.offset, event);
  }
  ++prv->offset;
  if (result == DOTS_DETAIL_EVT_LEB128_READER_MORE) {
    return 1;
  }
  switch (prv->state) {
  case DOTS_DETAIL_EVT_STATE_MAJOR:
    if (value != DOTS_SPEC_MAJOR) {
      return dots_detail_evt_fail(prv, DOTS_UNSUPPORTED_VERSION,
                                  prv->leb128.offset, event);
    }
    prv->header.version.major = value;
    prv->state = DOTS_DETAIL_EVT_STATE_MINOR;
    break;
  case DOTS_DETAIL_EVT_STATE_MINOR:
    if (value != DOTS_SPEC_MINOR) {
      return dots_detail_evt_fail(prv, DOTS_UNSUPPORTED_VERSION,
                                  prv->leb128.offset, event);
    }
    prv->header.version.minor = value;
    prv->state = DOTS_DETAIL_EVT_STATE_UPDATE;
    break;
  case DOTS_DETAIL_EVT_STATE_UPDATE:
    prv->header.version.update = value;
    prv->state = DOTS_DETAIL_EVT_STATE_FLAGS;
    break;
  case DOTS_DETAIL_EVT_STATE_FLAGS:
    if (value > 1) {
      return dots_detail_evt_fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset,
                                  event);
    }
    prv->header.flags = value;
    prv->state = DOTS_DETAIL_EVT_STATE_COLUMNS;
    break;
  case DOTS_DETAIL_EVT_STATE_COLUMNS:
    if (value == 0) {
      return dots_detail_evt_fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset,
                                  event);
    }
    prv->header.dimensions.x = value;
    prv->state = DOTS_DETAIL_EVT_STATE_ROWS;
    break;
  case DOTS_DETAIL_EVT_STATE_ROWS:
    if (value == 0) {
      return dots_detail_evt_fail(prv, DOTS_INVALID_VALUE, prv->leb128.offset,
                                  event);
    }
    prv->header.dimensions.y = value;
    prv->state = DOTS_DETAIL_EVT_STATE_FRAME_RATE;
    break;
  case DOTS_DETAIL_EVT_STATE_FRAME_RATE:
    prv->header.frame_rate = value;
    prv->state = DOTS_DETAIL_EVT_STATE_FRAME;
    event->type = DOTS_EVT_EVENT_HEADER;
    event->offset = 0;
    event->header = prv->header;
    break;
  case DOTS_DETAIL_EVT_STATE_X:
    if (value >= prv->header.dimensions.x) {
      return dots_detail_evt_fail(prv, DOTS_OUTSIDE_GRID, prv->leb128.offset,
                                  event);
    }
    prv->run.head.x = value;
    prv->run.cells_left = prv->header.dimensions.x - value;
    prv->state = DOTS_DETAIL_EVT_STATE_Y;
    break;
  case DOTS_DETAIL_EVT_STATE_Y:
    if (value >= prv->header.dimensions.y) {
      return dots_detail_evt_fail(prv, DOTS_OUTSIDE_GRID, prv->leb128.offset,
                                  event);
    }
    prv->run.head.y = value;
    prv->state = DOTS_DETAIL_EVT_STATE_COUNT;
    break;
  case DOTS_DETAIL_EVT_STATE_COUNT:
    prv->run.head.count = value;
    prv->run.data_left = value;
    prv->state =
        value == 0 ? dots_detail_evt_layer(prv) : DOTS_DETAIL_EVT_STATE_DATA;
    event->type = DOTS_EVT_EVENT_RUN;
    event->offset = prv->run.offset;
    event->run = prv->run.head;
    break;
  case DOTS_DETAIL_EVT_STATE_FOURCC:
  case DOTS_DETAIL_EVT_STATE_FRAME:
  case DOTS_DETAIL_EVT_STATE_BLOCKS:
  case DOTS_DETAIL_EVT_STATE_TEXT:
  case DOTS_DETAIL_EVT_STATE_DATA:
  case DOTS_DETAIL_EVT_STATE_END:
  case DOTS_DETAIL_EVT_STATE_ERROR:
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
static size_t dots_detail_evt_read_frame(struct dots_detail_evt_reader_prv *prv,
                                         unsigned char octet,
                                         struct dots_evt_event *event) {
  if (octet == DOTS_DETAIL_EVT_END_OF_STREAM) {
    return dots_detail_evt_token(prv, DOTS_EVT_EVENT_END_OF_STREAM,
                                 DOTS_DETAIL_EVT_STATE_END, event);
  }
  if (octet == DOTS_DETAIL_EVT_CLEAR) {
    event->clear = true;
    return dots_detail_evt_token(prv, DOTS_EVT_EVENT_FRAME,
                                 DOTS_DETAIL_EVT_STATE_BLOCKS, event);
  }
  if (octet != DOTS_DETAIL_EVT_START_OF_RUN &&
      octet != DOTS_DETAIL_EVT_END_OF_BLOCKS) {
    return dots_detail_evt_fail(prv, DOTS_MALFORMED, prv->offset, event);
  }
  event->type = DOTS_EVT_EVENT_FRAME;
  event->offset = prv->offset;
  prv->state = DOTS_DETAIL_EVT_STATE_BLOCKS;
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
static size_t dots_detail_evt_read_layer(struct dots_detail_evt_reader_prv *prv,
                                         unsigned char octet,
                                         struct dots_evt_event *event) {
  bool text = prv->state == DOTS_DETAIL_EVT_STATE_TEXT;

  if (octet == DOTS_DETAIL_EVT_START_OF_RUN) {
    if (text && !(prv->header.flags & 1)) {
      return dots_detail_evt_fail(prv, DOTS_UNEXPECTED_TEXT, prv->offset,
                                  event);
    }
    prv->run.head.text = text;
    prv->run.offset = prv->offset;
    prv->state = DOTS_DETAIL_EVT_STATE_X;
    ++prv->offset;
    return 1;
  }
  if (!text && octet == DOTS_DETAIL_EVT_END_OF_BLOCKS) {
    return dots_detail_evt_token(prv, DOTS_EVT_EVENT_END_OF_BLOCKS,
                                 DOTS_DETAIL_EVT_STATE_TEXT, event);
  }
  if (text && octet == DOTS_DETAIL_EVT_END_OF_TEXT) {
    return dots_detail_evt_token(prv, DOTS_EVT_EVENT_END_OF_TEXT,
                                 DOTS_DETAIL_EVT_STATE_FRAME, event);
  }
  return dots_detail_evt_fail(prv, DOTS_MALFORMED, prv->offset, event);
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
static size_t
dots_detail_evt_read_blocks(struct dots_detail_evt_reader_prv *prv,
                            const unsigned char *data, size_t size,
                            struct dots_evt_event *event) {
  uint64_t limit = prv->run.data_left < prv->run.cells_left
                       ? prv->run.data_left
                       : prv->run.cells_left;
  size_t used = limit < size ? (size_t)limit : size;

  if (limit == 0) {
    return dots_detail_evt_fail(prv, DOTS_OUTSIDE_GRID, prv->offset, event);
  }
  event->type = DOTS_EVT_EVENT_CHUNK;
  event->offset = prv->offset;
  event->chunk.octets = data;
  event->chunk.size = used;
  prv->run.data_left -= used;
  prv->run.cells_left -= used;
  prv->offset += used;
  prv->state = prv->run.data_left == 0 ? dots_detail_evt_layer(prv)
                                       : DOTS_DETAIL_EVT_STATE_DATA;
  return used;
}

/**
 * The length of a UTF-8 sequence, from its first octet (Section 4 of
 * RFC 3629).
 *
 * @param lead the first octet
 * @return the length, from 1 to 4, or 0 if no sequence starts with lead
 */
static unsigned dots_detail_evt_utf8_length(unsigned char lead) {
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
 * @param length its length, from 1 to 4, as dots_detail_evt_utf8_length gives
 * it
 * @return its Unicode scalar value
 */
static uint32_t dots_detail_evt_utf8_decode(const unsigned char *octets,
                                            unsigned length) {
  static const unsigned char lead_bits[DOTS_DETAIL_UTF8_MAX] = {0x7F, 0x1F,
                                                                0x0F, 0x07};
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
static enum dots_detail_evt_utf8_result
dots_detail_evt_utf8_push(struct dots_detail_evt_utf8 *utf8,
                          unsigned char octet, uint64_t offset) {
  unsigned char low = 0x80;
  unsigned char high = 0xBF;

  if (utf8->size == utf8->length) {
    utf8->size = 0;
    utf8->length = dots_detail_evt_utf8_length(octet);
    utf8->offset = offset;
    if (utf8->length == 0) {
      return DOTS_DETAIL_EVT_UTF8_INVALID;
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
      return DOTS_DETAIL_EVT_UTF8_INVALID;
    }
  }
  utf8->octets[utf8->size] = octet;
  ++utf8->size;
  return utf8->size < utf8->length ? DOTS_DETAIL_EVT_UTF8_MORE
                                   : DOTS_DETAIL_EVT_UTF8_DONE;
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
static size_t dots_detail_evt_read_text(struct dots_detail_evt_reader_prv *prv,
                                        const unsigned char *data, size_t size,
                                        struct dots_evt_event *event) {
  size_t end = prv->run.data_left < size ? (size_t)prv->run.data_left : size;
  bool carried = prv->utf8.size < prv->utf8.length;
  uint64_t start = prv->offset;
  size_t used = 0;
  size_t whole = 0;
  enum dots_code code = DOTS_OK;

  while (used < end && code == DOTS_OK) {
    enum dots_detail_evt_utf8_result result =
        dots_detail_evt_utf8_push(&prv->utf8, data[used], prv->offset);

    if (result == DOTS_DETAIL_EVT_UTF8_INVALID) {
      code = DOTS_INVALID_TEXT;
    } else if (result == DOTS_DETAIL_EVT_UTF8_DONE &&
               prv->run.cells_left == 0) {
      code = DOTS_OUTSIDE_GRID;
    } else {
      ++used;
      ++prv->offset;
      --prv->run.data_left;
      if (result == DOTS_DETAIL_EVT_UTF8_DONE) {
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
    return dots_detail_evt_fail(prv, code, prv->utf8.offset, event);
  }
  if (code != DOTS_OK) { /* the next call reads the rest again */
    prv->offset = start + whole;
    prv->run.data_left += used - whole;
    prv->utf8.size = 0;
    prv->utf8.length = 0;
    used = whole;
  }
  prv->state = prv->run.data_left == 0 ? dots_detail_evt_layer(prv)
                                       : DOTS_DETAIL_EVT_STATE_DATA;
  if (whole == 0) {
    return used; /* no event: a sequence goes on past the input */
  }
  event->type = DOTS_EVT_EVENT_CHUNK;
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

void dots_evt_reader_init(struct dots_evt_reader *reader) {
  static const struct dots_evt_reader initial = DOTS_DETAIL_ZERO;

  *reader = initial;
}

size_t dots_evt_reader_feed(struct dots_evt_reader *reader,
                            const unsigned char *data, size_t size,
                            struct dots_evt_event *event) {
  static const struct dots_evt_event none = DOTS_DETAIL_ZERO;
  struct dots_detail_evt_reader_prv *prv = &reader->prv;
  size_t used = 0;

  if (prv->state == DOTS_DETAIL_EVT_STATE_ERROR) {
    *event = prv->error;
    return 0;
  }
  *event = none;
  if (prv->state == DOTS_DETAIL_EVT_STATE_END) {
    return size; /* STR-2 */
  }
  while (used < size && event->type == DOTS_EVT_EVENT_NONE) {
    unsigned char octet = data[used];

    switch (prv->state) {
    case DOTS_DETAIL_EVT_STATE_FOURCC:
      used += dots_detail_evt_read_fourcc(prv, octet, event);
      break;
    case DOTS_DETAIL_EVT_STATE_MAJOR:
    case DOTS_DETAIL_EVT_STATE_MINOR:
    case DOTS_DETAIL_EVT_STATE_UPDATE:
    case DOTS_DETAIL_EVT_STATE_FLAGS:
    case DOTS_DETAIL_EVT_STATE_COLUMNS:
    case DOTS_DETAIL_EVT_STATE_ROWS:
    case DOTS_DETAIL_EVT_STATE_FRAME_RATE:
    case DOTS_DETAIL_EVT_STATE_X:
    case DOTS_DETAIL_EVT_STATE_Y:
    case DOTS_DETAIL_EVT_STATE_COUNT:
      used += dots_detail_evt_read_field(prv, octet, event);
      break;
    case DOTS_DETAIL_EVT_STATE_FRAME:
      used += dots_detail_evt_read_frame(prv, octet, event);
      break;
    case DOTS_DETAIL_EVT_STATE_BLOCKS:
    case DOTS_DETAIL_EVT_STATE_TEXT:
      used += dots_detail_evt_read_layer(prv, octet, event);
      break;
    case DOTS_DETAIL_EVT_STATE_DATA:
      if (prv->run.head.text) {
        used += dots_detail_evt_read_text(prv, data + used, size - used, event);
      } else {
        used +=
            dots_detail_evt_read_blocks(prv, data + used, size - used, event);
      }
      break;
    case DOTS_DETAIL_EVT_STATE_END:
    case DOTS_DETAIL_EVT_STATE_ERROR:
      break; /* not reached: an event ends the loop */
    }
  }
  return used;
}

void dots_evt_reader_finish(struct dots_evt_reader *reader,
                            struct dots_evt_event *event) {
  static const struct dots_evt_event none = DOTS_DETAIL_ZERO;
  struct dots_detail_evt_reader_prv *prv = &reader->prv;

  if (prv->state == DOTS_DETAIL_EVT_STATE_ERROR) {
    *event = prv->error;
    return;
  }
  *event = none;
  if (prv->state == DOTS_DETAIL_EVT_STATE_END) {
    event->type = DOTS_EVT_EVENT_END_OF_STREAM;
    event->offset = prv->offset - 1; /* the end-of-stream token */
    return;
  }
  event->type = DOTS_EVT_EVENT_ERROR;
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
static size_t dots_detail_evt_leb128_write(uint64_t value, unsigned char *out) {
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
 * @param[out] out where its octets go: room for DOTS_EVT_WRITER_MAX
 * @return the number of octets
 */
static size_t dots_detail_evt_encode(const struct dots_evt_event *event,
                                     unsigned char *out) {
  size_t size = 0;
  size_t i;

  switch (event->type) {
  case DOTS_EVT_EVENT_HEADER:
    for (i = 0; i < sizeof dots_detail_evt_fourcc; ++i) {
      out[i] = dots_detail_evt_fourcc[i];
    }
    size = sizeof dots_detail_evt_fourcc;
    size +=
        dots_detail_evt_leb128_write(event->header.version.major, out + size);
    size +=
        dots_detail_evt_leb128_write(event->header.version.minor, out + size);
    size +=
        dots_detail_evt_leb128_write(event->header.version.update, out + size);
    size += dots_detail_evt_leb128_write(event->header.flags, out + size);
    size +=
        dots_detail_evt_leb128_write(event->header.dimensions.x, out + size);
    size +=
        dots_detail_evt_leb128_write(event->header.dimensions.y, out + size);
    size += dots_detail_evt_leb128_write(event->header.frame_rate, out + size);
    break;
  case DOTS_EVT_EVENT_FRAME:
    if (event->clear) {
      out[0] = DOTS_DETAIL_EVT_CLEAR;
      size = 1;
    }
    break;
  case DOTS_EVT_EVENT_RUN:
    out[0] = DOTS_DETAIL_EVT_START_OF_RUN;
    size = 1;
    size += dots_detail_evt_leb128_write(event->run.x, out + size);
    size += dots_detail_evt_leb128_write(event->run.y, out + size);
    size += dots_detail_evt_leb128_write(event->run.count, out + size);
    break;
  case DOTS_EVT_EVENT_END_OF_BLOCKS:
    out[0] = DOTS_DETAIL_EVT_END_OF_BLOCKS;
    size = 1;
    break;
  case DOTS_EVT_EVENT_END_OF_TEXT:
    out[0] = DOTS_DETAIL_EVT_END_OF_TEXT;
    size = 1;
    break;
  case DOTS_EVT_EVENT_END_OF_STREAM:
    out[0] = DOTS_DETAIL_EVT_END_OF_STREAM;
    size = 1;
    break;
  case DOTS_EVT_EVENT_NONE:
  case DOTS_EVT_EVENT_CHUNK:
  case DOTS_EVT_EVENT_ERROR:
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
 * @return whether it may
 */
static bool
dots_detail_evt_writer_allows(const struct dots_detail_evt_writer_prv *prv,
                              const struct dots_evt_event *event) {
  enum dots_detail_evt_state state =
      prv->frame ? DOTS_DETAIL_EVT_STATE_BLOCKS : prv->reader.prv.state;

  switch (event->type) {
  case DOTS_EVT_EVENT_HEADER:
    return state == DOTS_DETAIL_EVT_STATE_FOURCC;
  case DOTS_EVT_EVENT_FRAME:
  case DOTS_EVT_EVENT_END_OF_STREAM:
    return state == DOTS_DETAIL_EVT_STATE_FRAME;
  case DOTS_EVT_EVENT_RUN:
    return event->run.text ? state == DOTS_DETAIL_EVT_STATE_TEXT
                           : state == DOTS_DETAIL_EVT_STATE_BLOCKS;
  case DOTS_EVT_EVENT_CHUNK:
    return state == DOTS_DETAIL_EVT_STATE_DATA;
  case DOTS_EVT_EVENT_END_OF_BLOCKS:
    return state == DOTS_DETAIL_EVT_STATE_BLOCKS;
  case DOTS_EVT_EVENT_END_OF_TEXT:
    return state == DOTS_DETAIL_EVT_STATE_TEXT;
  case DOTS_EVT_EVENT_NONE:
  case DOTS_EVT_EVENT_ERROR:
    return false;
  }
  return false;
}

/**
 * Feeds octets to a reader, all of them unless it gives an error.
 *
 * @param reader the reader
 * @param octets the octets
 * @param size the number of octets
 * @return DOTS_OK, or the code of the error
 */
static enum dots_code dots_detail_evt_check(struct dots_evt_reader *reader,
                                            const unsigned char *octets,
                                            size_t size) {
  struct dots_evt_event event;
  size_t used = 0;

  while (used < size) {
    used += dots_evt_reader_feed(reader, octets + used, size - used, &event);
    if (event.type == DOTS_EVT_EVENT_ERROR) {
      return event.code;
    }
  }
  return DOTS_OK;
}

/**
 * Whether the current run can still end well-formed: its data left
 * fits the cells left in its row (DEC-5), and in the text layer
 * completes the sequence in progress (DEC-3). A character takes at
 * most DOTS_DETAIL_UTF8_MAX octets.
 *
 * @param prv the reader's state
 * @return DOTS_OK, or the code of the requirement no data can satisfy
 */
static enum dots_code
dots_detail_evt_completable(const struct dots_detail_evt_reader_prv *prv) {
  uint64_t data = prv->run.data_left;
  uint64_t cells = prv->run.cells_left;
  uint64_t fewest; /* the fewest characters data octets make */

  if (prv->state != DOTS_DETAIL_EVT_STATE_DATA) {
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
  fewest = data / DOTS_DETAIL_UTF8_MAX + (data % DOTS_DETAIL_UTF8_MAX != 0);
  return fewest <= cells ? DOTS_OK : DOTS_OUTSIDE_GRID;
}

void dots_evt_writer_init(struct dots_evt_writer *writer) {
  static const struct dots_evt_writer initial = DOTS_DETAIL_ZERO;

  *writer = initial;
}

struct dots_evt_writer_result
dots_evt_writer_put(struct dots_evt_writer *writer,
                    const struct dots_evt_event *event, unsigned char *out,
                    size_t capacity) {
  static const struct dots_evt_writer_result none = DOTS_DETAIL_ZERO;
  struct dots_detail_evt_writer_prv *prv = &writer->prv;
  struct dots_evt_writer_result result = none;
  unsigned char encoded[DOTS_EVT_WRITER_MAX];
  const unsigned char *octets = encoded;
  struct dots_evt_reader reader;
  bool chunk = event->type == DOTS_EVT_EVENT_CHUNK;
  size_t size;
  size_t i;

  if (!dots_detail_evt_writer_allows(prv, event)) {
    result.status = DOTS_EVT_WRITER_OUT_OF_ORDER;
    return result;
  }
  if (chunk) {
    if (event->chunk.size > prv->reader.prv.run.data_left) {
      result.status = DOTS_EVT_WRITER_PAST_COUNT;
      return result;
    }
    octets = event->chunk.octets;
    size = event->chunk.size;
  } else {
    size = dots_detail_evt_encode(event, encoded);
  }
  if (!chunk && size > capacity) {
    result.status = DOTS_EVT_WRITER_NO_SPACE;
    return result;
  }
  reader = prv->reader;
  result.code = dots_detail_evt_check(&reader, octets, size);
  if (result.code == DOTS_OK) {
    result.code = dots_detail_evt_completable(&reader.prv);
  }
  if (result.code != DOTS_OK) {
    result.status = DOTS_EVT_WRITER_ILL_FORMED;
    return result;
  }
  prv->reader = reader;
  prv->frame = event->type == DOTS_EVT_EVENT_FRAME && !event->clear;
  if (!chunk) {
    for (i = 0; i < size; ++i) {
      out[i] = octets[i];
    }
    octets = out;
  }
  result.octets = octets;
  result.size = size;
  return result;
}

/**
 * Gives every cell the initial state: the block 0x00 and no character.
 *
 * @param prv the composer's state
 */
static void dots_detail_evt_clear(struct dots_detail_evt_composer_prv *prv) {
  static const struct dots_cell initial = DOTS_DETAIL_ZERO;
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
static void
dots_detail_evt_compose_blocks(struct dots_detail_evt_composer_prv *prv,
                               const struct dots_evt_chunk *chunk) {
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
static void
dots_detail_evt_compose_text(struct dots_detail_evt_composer_prv *prv,
                             const struct dots_evt_chunk *chunk) {
  size_t i = 0;

  while (i < chunk->size && prv->next < prv->columns * prv->rows) {
    unsigned length = dots_detail_evt_utf8_length(chunk->octets[i]);

    if (length == 0 || length > chunk->size - i) {
      return; /* not a chunk of the reader */
    }
    prv->cells[prv->next].character =
        dots_detail_evt_utf8_decode(chunk->octets + i, length);
    ++prv->next;
    i += length;
  }
}

bool dots_evt_composer_init(struct dots_evt_composer *composer,
                            const struct dots_header *header,
                            struct dots_cell *cells, size_t count) {
  struct dots_detail_evt_composer_prv *prv = &composer->prv;

  if (header->dimensions.y != 0 &&
      header->dimensions.x > count / header->dimensions.y) {
    return false;
  }
  prv->cells = cells;
  prv->columns = (size_t)header->dimensions.x;
  prv->rows = (size_t)header->dimensions.y;
  prv->next = 0;
  prv->text = false;
  dots_detail_evt_clear(prv);
  return true;
}

void dots_evt_composer_apply(struct dots_evt_composer *composer,
                             const struct dots_evt_event *event) {
  struct dots_detail_evt_composer_prv *prv = &composer->prv;

  switch (event->type) {
  case DOTS_EVT_EVENT_FRAME:
    if (event->clear) {
      dots_detail_evt_clear(prv);
    }
    break;
  case DOTS_EVT_EVENT_RUN:
    prv->text = event->run.text;
    if (event->run.x < prv->columns && event->run.y < prv->rows) {
      prv->next = (size_t)event->run.y * prv->columns + (size_t)event->run.x;
    } else {
      prv->next = prv->columns * prv->rows; /* nothing to set */
    }
    break;
  case DOTS_EVT_EVENT_CHUNK:
    if (prv->text) {
      dots_detail_evt_compose_text(prv, &event->chunk);
    } else {
      dots_detail_evt_compose_blocks(prv, &event->chunk);
    }
    break;
  case DOTS_EVT_EVENT_NONE:
  case DOTS_EVT_EVENT_HEADER:
  case DOTS_EVT_EVENT_END_OF_BLOCKS:
  case DOTS_EVT_EVENT_END_OF_TEXT:
  case DOTS_EVT_EVENT_END_OF_STREAM:
  case DOTS_EVT_EVENT_ERROR:
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

#ifdef DOTS_STDIO
#include <stdio.h>

int dots_mdl_stdio_get(void *handle) {
  FILE *file = (FILE *)handle;
  int c = fgetc(file);

  if (c == EOF) {
    return ferror(file) ? DOTS_MDL_IO_ERROR : DOTS_MDL_IO_END;
  }
  return c;
}

int dots_mdl_stdio_put(int octet, void *handle) {
  return fputc(octet, (FILE *)handle) == EOF ? DOTS_MDL_IO_ERROR : 0;
}
#endif /* DOTS_STDIO */

/**
 * A result with a status only.
 *
 * @param status the status
 * @return the result
 */
static struct dots_mdl_result
dots_detail_mdl_result(enum dots_mdl_status status) {
  struct dots_mdl_result result = DOTS_DETAIL_ZERO;

  result.status = status;
  return result;
}

/**
 * Reads the next event, getting octets from the input as the reader
 * needs them.
 *
 * @param prv the source's state
 * @param[out] event the event, on DOTS_MDL_OK: never an error
 * @return the result: ILL_FORMED for an error of the reader
 */
static struct dots_mdl_result
dots_detail_mdl_next(struct dots_detail_mdl_source_prv *prv,
                     struct dots_evt_event *event) {
  struct dots_mdl_result result = dots_detail_mdl_result(DOTS_MDL_OK);

  do {
    if (prv->octet == DOTS_DETAIL_MDL_NO_OCTET) {
      int got = prv->input.get(prv->input.handle);

      if (got == DOTS_MDL_IO_END) {
        dots_evt_reader_finish(&prv->reader, event);
        break;
      }
      if (got == DOTS_MDL_IO_ERROR) {
        result.status = DOTS_MDL_IO_FAILED;
        return result;
      }
      if (got < 0 || got > 0xFF) {
        result.status = DOTS_MDL_BAD_INPUT;
        return result;
      }
      prv->octet = got;
    }
    {
      unsigned char octet = (unsigned char)prv->octet;

      if (dots_evt_reader_feed(&prv->reader, &octet, 1, event) == 1) {
        prv->octet = DOTS_DETAIL_MDL_NO_OCTET;
      }
    }
  } while (event->type == DOTS_EVT_EVENT_NONE);
  if (event->type == DOTS_EVT_EVENT_ERROR) {
    result.status = DOTS_MDL_ILL_FORMED;
    result.code = event->code;
    result.offset = event->offset;
  }
  return result;
}

/**
 * Frees the runs of a list.
 *
 * @param run the first run, or NULL
 * @param release the free of the allocator
 */
static void dots_detail_mdl_release_runs(struct dots_mdl_run *run,
                                         void (*release)(void *)) {
  while (run != NULL) {
    struct dots_mdl_run *next = run->next;

    release(run->data);
    release(run);
    run = next;
  }
}

/**
 * Allocates a run, from its count alone: the reader checks its data
 * against its row (DEC-5) as the data arrives.
 *
 * @param prv the source's state
 * @param head the start of the run
 * @param[out] run the run, its data not yet read
 * @return the result: NO_MEMORY or OK
 */
static struct dots_mdl_result
dots_detail_mdl_new_run(struct dots_detail_mdl_source_prv *prv,
                        const struct dots_evt_run *head,
                        struct dots_mdl_run **run) {
  uint64_t count = head->count;
  struct dots_mdl_run *made;

  if ((uint64_t)(size_t)count != count) {
    return dots_detail_mdl_result(DOTS_MDL_NO_MEMORY);
  }
  made = (struct dots_mdl_run *)prv->allocator.allocate(sizeof *made);
  if (made == NULL) {
    return dots_detail_mdl_result(DOTS_MDL_NO_MEMORY);
  }
  made->position.x = head->x;
  made->position.y = head->y;
  made->count = count;
  made->data = NULL;
  made->next = NULL;
  if (count > 0) {
    made->data = (unsigned char *)prv->allocator.allocate((size_t)count);
    if (made->data == NULL) {
      prv->allocator.free(made);
      return dots_detail_mdl_result(DOTS_MDL_NO_MEMORY);
    }
  }
  *run = made;
  return dots_detail_mdl_result(DOTS_MDL_OK);
}

/**
 * Sets every member of a frame.
 *
 * @param frame the frame
 * @param clear the frame starts with a clear
 * @param block_runs the first run of the block layer, or NULL
 * @param text_runs the first run of the text layer, or NULL
 */
static void dots_detail_mdl_init_frame(struct dots_mdl_frame *frame, bool clear,
                                       struct dots_mdl_run *block_runs,
                                       struct dots_mdl_run *text_runs) {
  frame->clear = clear;
  frame->blocks.runs = block_runs;
  frame->text.runs = text_runs;
}

/**
 * Reads a frame after its start, up to its end of text.
 *
 * @param prv the source's state
 * @param clear the frame starts with a clear
 * @param[out] frame the frame, on DOTS_MDL_OK
 * @return the result: on any status but OK, nothing is left allocated
 */
static struct dots_mdl_result
dots_detail_mdl_read_frame(struct dots_detail_mdl_source_prv *prv, bool clear,
                           struct dots_mdl_frame **frame) {
  struct dots_mdl_frame *made =
      (struct dots_mdl_frame *)prv->allocator.allocate(sizeof *made);
  struct dots_mdl_run **tail;
  struct dots_mdl_run *run = NULL; /* the run read */
  size_t filled = 0;
  struct dots_evt_event event;
  struct dots_mdl_result result;

  if (made == NULL) {
    return dots_detail_mdl_result(DOTS_MDL_NO_MEMORY);
  }
  dots_detail_mdl_init_frame(made, clear, NULL, NULL);
  tail = &made->blocks.runs;
  for (;;) {
    result = dots_detail_mdl_next(prv, &event);
    if (result.status != DOTS_MDL_OK) {
      break;
    }
    if (event.type == DOTS_EVT_EVENT_END_OF_TEXT) {
      *frame = made;
      return result;
    }
    if (event.type == DOTS_EVT_EVENT_END_OF_BLOCKS) {
      tail = &made->text.runs;
    } else if (event.type == DOTS_EVT_EVENT_RUN) {
      result = dots_detail_mdl_new_run(prv, &event.run, &run);
      if (result.status != DOTS_MDL_OK) {
        break;
      }
      *tail = run;
      tail = &run->next;
      filled = 0;
    } else if (event.type == DOTS_EVT_EVENT_CHUNK) {
      size_t i;

      for (i = 0; i < event.chunk.size; ++i) {
        run->data[filled + i] = event.chunk.octets[i];
      }
      filled += event.chunk.size;
    }
  }
  dots_detail_mdl_release_runs(made->blocks.runs, prv->allocator.free);
  dots_detail_mdl_release_runs(made->text.runs, prv->allocator.free);
  prv->allocator.free(made);
  return result;
}

struct dots_mdl_result dots_mdl_open_source(struct dots_mdl_source *source,
                                            struct dots_mdl_allocator allocator,
                                            struct dots_mdl_input input,
                                            struct dots_header *header) {
  struct dots_detail_mdl_source_prv *prv = &source->prv;
  struct dots_evt_event event;

  dots_evt_reader_init(&prv->reader);
  prv->allocator = allocator;
  prv->input = input;
  prv->octet = DOTS_DETAIL_MDL_NO_OCTET;
  prv->result = dots_detail_mdl_next(prv, &event);
  if (prv->result.status != DOTS_MDL_OK) {
    return prv->result;
  }
  *header = event.header; /* the reader gives the header or an error first */
  return prv->result;
}

struct dots_mdl_result dots_mdl_get_frame(struct dots_mdl_source *source,
                                          struct dots_mdl_frame **frame) {
  struct dots_detail_mdl_source_prv *prv = &source->prv;
  struct dots_evt_event event;
  struct dots_mdl_result result;

  *frame = NULL;
  if (prv->result.status != DOTS_MDL_OK) {
    return prv->result;
  }
  result = dots_detail_mdl_next(prv, &event);
  if (result.status == DOTS_MDL_OK) {
    if (event.type == DOTS_EVT_EVENT_END_OF_STREAM) {
      result.status = DOTS_MDL_END;
    } else { /* the start of a frame */
      result = dots_detail_mdl_read_frame(prv, event.clear, frame);
    }
  }
  prv->result = result.status == DOTS_MDL_OK ? prv->result : result;
  return result;
}

void dots_mdl_release_frame(struct dots_mdl_source *source,
                            struct dots_mdl_frame *frame) {
  void (*release)(void *) = source->prv.allocator.free;

  if (frame == NULL) {
    return;
  }
  dots_detail_mdl_release_runs(frame->blocks.runs, release);
  dots_detail_mdl_release_runs(frame->text.runs, release);
  release(frame);
}

/**
 * Writes an event and puts its octets.
 *
 * @param prv the sink's state
 * @param event the event
 * @return the result: ILL_FORMED with the writer's code
 */
static struct dots_mdl_result
dots_detail_mdl_put(struct dots_detail_mdl_sink_prv *prv,
                    const struct dots_evt_event *event) {
  unsigned char out[DOTS_EVT_WRITER_MAX];
  struct dots_evt_writer_result written =
      dots_evt_writer_put(&prv->writer, event, out, sizeof out);
  struct dots_mdl_result result = dots_detail_mdl_result(DOTS_MDL_OK);
  size_t i;

  if (written.status == DOTS_EVT_WRITER_ILL_FORMED) {
    result.status = DOTS_MDL_ILL_FORMED;
    result.code = written.code;
    return result;
  }
  if (written.status != DOTS_EVT_WRITER_OK) {
    /* not reached: events in order, each run's data in one chunk */
    result.status = DOTS_MDL_OUT_OF_ORDER;
    return result;
  }
  for (i = 0; i < written.size; ++i) {
    if (prv->output.put(written.octets[i], prv->output.handle) != 0) {
      result.status = DOTS_MDL_IO_FAILED;
      return result;
    }
  }
  return result;
}

/**
 * Writes the runs of a layer, then its end.
 *
 * @param prv the sink's state
 * @param run the first run, or NULL
 * @param text true for the text layer, false for the block layer
 * @return the result
 */
static struct dots_mdl_result
dots_detail_mdl_put_layer(struct dots_detail_mdl_sink_prv *prv,
                          const struct dots_mdl_run *run, bool text) {
  struct dots_evt_event event = DOTS_DETAIL_ZERO;
  struct dots_mdl_result result = dots_detail_mdl_result(DOTS_MDL_OK);

  for (; run != NULL && result.status == DOTS_MDL_OK; run = run->next) {
    event.type = DOTS_EVT_EVENT_RUN;
    event.run.text = text;
    event.run.x = run->position.x;
    event.run.y = run->position.y;
    event.run.count = run->count;
    result = dots_detail_mdl_put(prv, &event);
    if (result.status == DOTS_MDL_OK && run->count > 0) {
      event.type = DOTS_EVT_EVENT_CHUNK;
      event.chunk.octets = run->data;
      event.chunk.size = (size_t)run->count; /* its data is in memory */
      result = dots_detail_mdl_put(prv, &event);
    }
  }
  if (result.status == DOTS_MDL_OK) {
    event.type =
        text ? DOTS_EVT_EVENT_END_OF_TEXT : DOTS_EVT_EVENT_END_OF_BLOCKS;
    result = dots_detail_mdl_put(prv, &event);
  }
  return result;
}

struct dots_mdl_result dots_mdl_open_sink(struct dots_mdl_sink *sink,
                                          struct dots_mdl_output output,
                                          const struct dots_header *header) {
  struct dots_detail_mdl_sink_prv *prv = &sink->prv;
  struct dots_evt_event event = DOTS_DETAIL_ZERO;

  dots_evt_writer_init(&prv->writer);
  prv->output = output;
  event.type = DOTS_EVT_EVENT_HEADER;
  event.header = *header;
  prv->result = dots_detail_mdl_put(prv, &event);
  return prv->result;
}

struct dots_mdl_result dots_mdl_put_frame(struct dots_mdl_sink *sink,
                                          const struct dots_mdl_frame *frame) {
  struct dots_detail_mdl_sink_prv *prv = &sink->prv;
  struct dots_evt_event event = DOTS_DETAIL_ZERO;
  struct dots_mdl_result result;

  if (prv->result.status != DOTS_MDL_OK) {
    return prv->result;
  }
  event.type = DOTS_EVT_EVENT_FRAME;
  event.clear = frame->clear;
  result = dots_detail_mdl_put(prv, &event);
  if (result.status == DOTS_MDL_OK) {
    result = dots_detail_mdl_put_layer(prv, frame->blocks.runs, false);
  }
  if (result.status == DOTS_MDL_OK) {
    result = dots_detail_mdl_put_layer(prv, frame->text.runs, true);
  }
  prv->result = result;
  return result;
}

struct dots_mdl_result dots_mdl_close_sink(struct dots_mdl_sink *sink) {
  struct dots_detail_mdl_sink_prv *prv = &sink->prv;
  struct dots_evt_event event = DOTS_DETAIL_ZERO;
  struct dots_mdl_result result;

  if (prv->result.status != DOTS_MDL_OK) {
    return prv->result;
  }
  event.type = DOTS_EVT_EVENT_END_OF_STREAM;
  result = dots_detail_mdl_put(prv, &event);
  prv->result = result.status == DOTS_MDL_OK
                    ? dots_detail_mdl_result(DOTS_MDL_OUT_OF_ORDER)
                    : result;
  return result;
}

/**
 * Replays the runs of a layer, then its end, to a composer.
 *
 * @param composer the composer
 * @param run the first run, or NULL
 * @param text true for the text layer, false for the block layer
 */
static void dots_detail_mdl_compose_layer(struct dots_evt_composer *composer,
                                          const struct dots_mdl_run *run,
                                          bool text) {
  struct dots_evt_event event = DOTS_DETAIL_ZERO;

  for (; run != NULL; run = run->next) {
    event.type = DOTS_EVT_EVENT_RUN;
    event.run.text = text;
    event.run.x = run->position.x;
    event.run.y = run->position.y;
    event.run.count = run->count;
    dots_evt_composer_apply(composer, &event);
    if (run->count > 0) {
      event.type = DOTS_EVT_EVENT_CHUNK;
      event.chunk.octets = run->data;
      event.chunk.size = (size_t)run->count; /* its data is in memory */
      dots_evt_composer_apply(composer, &event);
    }
  }
  event.type = text ? DOTS_EVT_EVENT_END_OF_TEXT : DOTS_EVT_EVENT_END_OF_BLOCKS;
  dots_evt_composer_apply(composer, &event);
}

void dots_mdl_compose(struct dots_evt_composer *composer,
                      const struct dots_mdl_frame *frame) {
  struct dots_evt_event event = DOTS_DETAIL_ZERO;

  event.type = DOTS_EVT_EVENT_FRAME;
  event.clear = frame->clear;
  dots_evt_composer_apply(composer, &event);
  dots_detail_mdl_compose_layer(composer, frame->blocks.runs, false);
  dots_detail_mdl_compose_layer(composer, frame->text.runs, true);
}

#endif /* DOTS_IMPLEMENTATION */
