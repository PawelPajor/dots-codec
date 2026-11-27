/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * Reads a stream at the model level and prints each frame: one line
 * per row, a cell's character, else its block in Braille.
 *
 * Usage: model_read <file.dots>
 */

#define DOTS_IMPLEMENTATION
#define DOTS_STDIO
#include <dots.h>

#include "common_read.h"

#include <stdio.h>
#include <stdlib.h>

/* Prints what went wrong, unless the stream has ended. */
static int report(struct dots_mdl_result result) {
  switch (result.status) {
  case DOTS_MDL_OK:
  case DOTS_MDL_END:
    return EXIT_SUCCESS;
  case DOTS_MDL_ILL_FORMED:
    fprintf(stderr, "ill-formed: %s at offset %llu\n",
            dots_code_name(result.code), (unsigned long long)result.offset);
    break;
  case DOTS_MDL_NO_MEMORY:
    fputs("out of memory\n", stderr);
    break;
  case DOTS_MDL_IO_FAILED:
    fputs("cannot read the file\n", stderr);
    break;
  default:
    fputs("cannot read the stream\n", stderr);
    break;
  }
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
  struct dots_mdl_allocator allocator = {malloc, free};
  struct dots_mdl_input input;
  struct dots_mdl_source source;
  struct dots_mdl_result result;
  struct dots_header header;
  struct dots_mdl_frame *frame;
  struct player player;
  FILE *file;

  if (argc != 2) {
    fputs("usage: model_read <file.dots>\n", stderr);
    return EXIT_FAILURE;
  }
  file = fopen(argv[1], "rb");
  if (file == NULL) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }
  input.get = dots_mdl_stdio_get;
  input.handle = file;

  result = dots_mdl_open_source(&source, allocator, input, &header);
  if (result.status != DOTS_MDL_OK) {
    fclose(file);
    return report(result);
  }

  if (!player_start(&player, &header)) {
    free(player.cells);
    fclose(file);
    return EXIT_FAILURE;
  }

  /* Each frame: compose it into the cells, print them, release it. */
  for (;;) {
    result = dots_mdl_get_frame(&source, &frame);
    if (result.status != DOTS_MDL_OK) {
      break;
    }
    dots_mdl_compose(&player.composer, frame);
    dots_mdl_release_frame(&source, frame);
    player_print(&player);
  }

  free(player.cells);
  fclose(file);
  return report(result);
}
