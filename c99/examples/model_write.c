/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * Writes a stream at the model level: three frames of blocks and text
 * on a grid of 16 x 4 cells. event_write.c writes the same octets.
 *
 * Usage: model_write <file.dots>
 */

#define DOTS_IMPLEMENTATION
#define DOTS_STDIO
#include <dots.h>

#include <stdio.h>
#include <stdlib.h>

/* Prints what went wrong. */
static int report(struct dots_mdl_result result) {
  if (result.status == DOTS_MDL_ILL_FORMED) {
    fprintf(stderr, "ill-formed: %s\n", dots_code_name(result.code));
  } else if (result.status == DOTS_MDL_IO_FAILED) {
    fputs("cannot write the file\n", stderr);
  } else {
    fputs("cannot write the stream\n", stderr);
  }
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
  /* Frame 1: four blocks at the top left, "dots" at the bottom left. */
  unsigned char blocks[] = {0xFF, 0xAA, 0x55, 0x0F};
  unsigned char dots[] = "dots";
  struct dots_mdl_run blocks_run = {{0, 0}, 4, blocks, NULL};
  struct dots_mdl_run dots_run = {{0, 3}, 4, dots, NULL};
  struct dots_mdl_frame first = {true, {&blocks_run}, {&dots_run}};

  /* Frame 2: no runs, it holds the state. */
  struct dots_mdl_frame hold = {false, {NULL}, {NULL}};

  /* Frame 3: a clear, then "Hi" and "C99" on the second row. */
  unsigned char hi[] = "Hi";
  unsigned char c99[] = "C99";
  struct dots_mdl_run c99_run = {{5, 1}, 3, c99, NULL};
  struct dots_mdl_run hi_run = {{1, 1}, 2, hi, &c99_run};
  struct dots_mdl_frame last = {true, {NULL}, {&hi_run}};

  const struct dots_mdl_frame *frames[] = {&first, &hold, &last};
  struct dots_header header = {{DOTS_SPEC_MAJOR, DOTS_SPEC_MINOR, 0},
                               1, /* contains-text */
                               {16, 4},
                               10};
  struct dots_mdl_output output;
  struct dots_mdl_sink sink;
  struct dots_mdl_result result;
  size_t i;
  FILE *file;

  if (argc != 2) {
    fputs("usage: model_write <file.dots>\n", stderr);
    return EXIT_FAILURE;
  }
  file = fopen(argv[1], "wb");
  if (file == NULL) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }
  output.put = dots_mdl_stdio_put;
  output.handle = file;

  /* The header, each frame, then end-of-stream. */
  result = dots_mdl_open_sink(&sink, output, &header);
  for (i = 0; i < sizeof frames / sizeof frames[0]; ++i) {
    if (result.status != DOTS_MDL_OK) {
      break;
    }
    result = dots_mdl_put_frame(&sink, frames[i]);
  }
  if (result.status == DOTS_MDL_OK) {
    result = dots_mdl_close_sink(&sink);
  }

  if (fclose(file) != 0 && result.status == DOTS_MDL_OK) {
    result.status = DOTS_MDL_IO_FAILED;
  }
  return result.status == DOTS_MDL_OK ? EXIT_SUCCESS : report(result);
}
