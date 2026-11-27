/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * Reads a stream at the event level and prints each frame: one line
 * per row, a cell's character, else its block in Braille.
 *
 * Usage: event_read <file.dots>
 */

#define DOTS_IMPLEMENTATION
#include <dots.h>

#include "common_read.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What the program does after an event. */
enum next { NEXT_MORE, NEXT_DONE, NEXT_FAIL };

/* Handles an event of the reader. */
static enum next handle(struct player *player,
                        const struct dots_evt_event *event) {
  switch (event->type) {
  case DOTS_EVT_EVENT_NONE:
    return NEXT_MORE;
  case DOTS_EVT_EVENT_HEADER:
    return player_start(player, &event->header) ? NEXT_MORE : NEXT_FAIL;
  case DOTS_EVT_EVENT_END_OF_STREAM:
    return NEXT_DONE;
  case DOTS_EVT_EVENT_ERROR:
    fprintf(stderr, "ill-formed: %s at offset %llu\n",
            dots_code_name(event->code), (unsigned long long)event->offset);
    return NEXT_FAIL;
  default:
    dots_evt_composer_apply(&player->composer, event);
    if (event->type == DOTS_EVT_EVENT_END_OF_TEXT) {
      player_print(player);
    }
    return NEXT_MORE;
  }
}

int main(int argc, char **argv) {
  unsigned char buffer[4096];
  struct dots_evt_reader reader;
  struct dots_evt_event event;
  struct player player;
  enum next next = NEXT_MORE;
  FILE *file;

  if (argc != 2) {
    fputs("usage: event_read <file.dots>\n", stderr);
    return EXIT_FAILURE;
  }
  file = fopen(argv[1], "rb");
  if (file == NULL) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }
  memset(&player, 0, sizeof player); /* no cells before the header */
  dots_evt_reader_init(&reader);

  /* Feed the file in buffers, an event per call, up to its end. */
  while (next == NEXT_MORE) {
    size_t size = fread(buffer, 1, sizeof buffer, file);
    const unsigned char *data = buffer;

    if (size == 0) {
      if (ferror(file)) {
        fputs("cannot read the file\n", stderr);
        next = NEXT_FAIL;
        break;
      }
      dots_evt_reader_finish(&reader, &event);
      next = handle(&player, &event);
      break;
    }
    while (size > 0 && next == NEXT_MORE) {
      size_t used = dots_evt_reader_feed(&reader, data, size, &event);

      data += used;
      size -= used;
      next = handle(&player, &event);
    }
  }

  free(player.cells);
  fclose(file);
  return next == NEXT_DONE ? EXIT_SUCCESS : EXIT_FAILURE;
}
