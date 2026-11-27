/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * What model_read.c and event_read.c share: the state of the grid, and
 * how it is printed. Include it after dots.h.
 */

#ifndef COMMON_READ_H
#define COMMON_READ_H

#include <dots.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The state of the grid, and what is printed of it. */
struct player {
  struct dots_evt_composer composer;
  struct dots_cell *cells; /* free them with free */
  size_t columns;
  size_t rows;
  bool first; /* no frame printed yet */
};

/* Prints a code point in UTF-8. */
static void print_utf8(uint32_t point) {
  if (point < 0x80) {
    putchar((int)point);
  } else if (point < 0x800) {
    putchar((int)(0xC0 | (point >> 6)));
    putchar((int)(0x80 | (point & 0x3F)));
  } else if (point < 0x10000) {
    putchar((int)(0xE0 | (point >> 12)));
    putchar((int)(0x80 | ((point >> 6) & 0x3F)));
    putchar((int)(0x80 | (point & 0x3F)));
  } else {
    putchar((int)(0xF0 | (point >> 18)));
    putchar((int)(0x80 | ((point >> 12) & 0x3F)));
    putchar((int)(0x80 | ((point >> 6) & 0x3F)));
    putchar((int)(0x80 | (point & 0x3F)));
  }
}

/*
 * Starts the grid of a stream: one cell per column and row, if they
 * fit in memory. Returns false, and says why, if they do not.
 */
static bool player_start(struct player *player,
                         const struct dots_header *header) {
  size_t count;

  player->cells = NULL;
  player->first = true;
  if (header->dimensions.x > SIZE_MAX || header->dimensions.y > SIZE_MAX) {
    fputs("the grid is too large\n", stderr);
    return false;
  }
  player->columns = (size_t)header->dimensions.x;
  player->rows = (size_t)header->dimensions.y;
  if (player->columns != 0 && player->rows > SIZE_MAX / player->columns) {
    fputs("the grid is too large\n", stderr);
    return false;
  }
  count = player->columns * player->rows;
  player->cells =
      (struct dots_cell *)calloc(count != 0 ? count : 1, sizeof *player->cells);
  if (player->cells == NULL) {
    fputs("out of memory\n", stderr);
    return false;
  }
  dots_evt_composer_init(&player->composer, header, player->cells, count);
  return true;
}

/*
 * Prints the cells, one line per row: a cell's character, else its
 * block in Braille. A blank line between frames.
 */
static void player_print(struct player *player) {
  size_t y;

  if (!player->first) {
    putchar('\n');
  }
  player->first = false;
  for (y = 0; y < player->rows; ++y) {
    size_t x;

    for (x = 0; x < player->columns; ++x) {
      const struct dots_cell *cell = &player->cells[y * player->columns + x];

      print_utf8(cell->character != 0 ? cell->character
                                      : dots_braille(cell->block));
    }
    putchar('\n');
  }
}

#endif /* COMMON_READ_H */
