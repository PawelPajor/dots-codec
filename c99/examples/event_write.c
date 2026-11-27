/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * Writes a stream at the event level: three frames of blocks and text
 * on a grid of 16 x 4 cells. model_write.c writes the same octets.
 *
 * Usage: event_write <file.dots>
 */

#define DOTS_IMPLEMENTATION
#include <dots.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Where the events go. */
struct output {
  struct dots_evt_writer writer;
  FILE *file;
  bool failed; /* an event was refused, or the file not written */
};

/* Writes an event, unless an earlier one failed. */
static void put(struct output *output, const struct dots_evt_event *event) {
  unsigned char octets[DOTS_EVT_WRITER_MAX];
  struct dots_evt_writer_result result;

  if (output->failed) {
    return;
  }
  result = dots_evt_writer_put(&output->writer, event, octets, sizeof octets);
  if (result.status != DOTS_EVT_WRITER_OK) {
    fprintf(stderr, "refused: status %d, %s\n", (int)result.status,
            result.status == DOTS_EVT_WRITER_ILL_FORMED
                ? dots_code_name(result.code)
                : "no code");
    output->failed = true;
  } else if (fwrite(result.octets, 1, result.size, output->file) !=
             result.size) {
    fputs("cannot write the file\n", stderr);
    output->failed = true;
  }
}

/* Writes a run and its data, as one chunk. */
static void put_run(struct output *output, struct dots_evt_event *event,
                    bool text, uint64_t x, uint64_t y, const char *data,
                    size_t size) {
  event->type = DOTS_EVT_EVENT_RUN;
  event->run.text = text;
  event->run.x = x;
  event->run.y = y;
  event->run.count = size;
  put(output, event);
  event->type = DOTS_EVT_EVENT_CHUNK;
  event->chunk.octets = (const unsigned char *)data;
  event->chunk.size = size;
  put(output, event);
}

/* Writes an event that only has a type. */
static void put_type(struct output *output, struct dots_evt_event *event,
                     enum dots_evt_event_type type) {
  event->type = type;
  put(output, event);
}

/* Writes the start of a frame. */
static void put_frame(struct output *output, struct dots_evt_event *event,
                      bool clear) {
  event->type = DOTS_EVT_EVENT_FRAME;
  event->clear = clear;
  put(output, event);
}

int main(int argc, char **argv) {
  struct output output;
  struct dots_evt_event event;

  if (argc != 2) {
    fputs("usage: event_write <file.dots>\n", stderr);
    return EXIT_FAILURE;
  }
  output.file = fopen(argv[1], "wb");
  if (output.file == NULL) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }
  output.failed = false;
  dots_evt_writer_init(&output.writer);

  /* The writer ignores the members a type does not name: one event. */
  memset(&event, 0, sizeof event);
  event.type = DOTS_EVT_EVENT_HEADER;
  event.header.version.major = DOTS_SPEC_MAJOR;
  event.header.version.minor = DOTS_SPEC_MINOR;
  event.header.flags = 1; /* contains-text */
  event.header.dimensions.x = 16;
  event.header.dimensions.y = 4;
  event.header.frame_rate = 10;
  put(&output, &event);

  /* Frame 1: four blocks at the top left, "dots" at the bottom left. */
  put_frame(&output, &event, true);
  put_run(&output, &event, false, 0, 0, "\xFF\xAA\x55\x0F", 4);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_BLOCKS);
  put_run(&output, &event, true, 0, 3, "dots", 4);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_TEXT);

  /* Frame 2: no runs, it holds the state. */
  put_frame(&output, &event, false);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_BLOCKS);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_TEXT);

  /* Frame 3: a clear, then "Hi" and "C99" on the second row. */
  put_frame(&output, &event, true);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_BLOCKS);
  put_run(&output, &event, true, 1, 1, "Hi", 2);
  put_run(&output, &event, true, 5, 1, "C99", 3);
  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_TEXT);

  put_type(&output, &event, DOTS_EVT_EVENT_END_OF_STREAM);

  if (fclose(output.file) != 0 && !output.failed) {
    fputs("cannot write the file\n", stderr);
    output.failed = true;
  }
  return output.failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
