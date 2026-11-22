/*
 * SPDX-License-Identifier: MIT OR Apache-2.0
 * Copyright (c) 2026 Pawel Pajor
 *
 * Conformance test for one example of the specification:
 *
 *   test_examples <listing.txt> <stream.dots>
 *
 * Checks that the stream holds exactly the octets of its listing, then
 * reads it in chunks of every size and checks that the reader returns
 * the result the first comment of the listing states.
 */

#define DOTS_IMPLEMENTATION
#include "dots.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OCTETS 4096

struct expected {
  enum dots_code code;
  uint64_t offset;
};

static int fail(const char *path, const char *what) {
  fprintf(stderr, "%s: %s\n", path, what);
  return 0;
}

static const char *name(enum dots_code code) {
  const char *name = dots_code_name(code);
  return name != 0 ? name : "an unknown code";
}

static int is_octet(const char *token) {
  return strlen(token) == 4 && token[0] == '0' && token[1] == 'x' &&
         isxdigit((unsigned char)token[2]) && isxdigit((unsigned char)token[3]);
}

/* "Accept: ..." or "Reject: CODE at offset N. ..." */
static int parse_expected(const char *text, struct expected *expected) {
  const char *reject = "Reject: ";
  char name[32];
  unsigned long long offset;
  int code;
  if (strncmp(text, "Accept:", 7) == 0) {
    expected->code = DOTS_OK;
    expected->offset = 0;
    return 1;
  }
  if (strncmp(text, reject, strlen(reject)) != 0 ||
      sscanf(text + strlen(reject), "%31s at offset %llu.", name, &offset) !=
          2) {
    return 0;
  }
  for (code = 1; dots_code_name((enum dots_code)code) != 0; ++code) {
    if (strcmp(dots_code_name((enum dots_code)code), name) == 0) {
      expected->code = (enum dots_code)code;
      expected->offset = offset;
      return 1;
    }
  }
  return 0;
}

/*
 * Reads the octets of a listing: everything outside comments, from
 * "//" to the end of the line, is octets in C notation separated by
 * commas and spaces. The expected result is in the first comment that
 * starts with "DOTS ".
 */
static int read_listing(const char *path, unsigned char *octets, size_t *size,
                        struct expected *expected) {
  char line[1024];
  int found = 0;
  FILE *file = fopen(path, "r");
  if (file == 0) {
    return fail(path, "cannot open");
  }
  *size = 0;
  while (fgets(line, sizeof line, file) != 0) {
    char *comment = strstr(line, "//");
    char *token;
    if (comment != 0) {
      const char *text = comment + 2;
      while (*text == ' ') {
        ++text;
      }
      if (!found && strncmp(text, "DOTS ", 5) == 0) {
        const char *result = strstr(text, ". ");
        if (result == 0 || !parse_expected(result + 2, expected)) {
          fclose(file);
          return fail(path, "no expected result in the first comment");
        }
        found = 1;
      }
      *comment = '\0';
    }
    for (token = strtok(line, ", \t\r\n"); token != 0;
         token = strtok(0, ", \t\r\n")) {
      if (!is_octet(token) || *size == MAX_OCTETS) {
        fclose(file);
        return fail(path, "not an octet, or too many");
      }
      octets[(*size)++] = (unsigned char)strtoul(token, 0, 16);
    }
  }
  fclose(file);
  return found ? 1 : fail(path, "no first comment starting with \"DOTS \"");
}

static int read_stream(const char *path, unsigned char *octets, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (file == 0) {
    return fail(path, "cannot open");
  }
  *size = fread(octets, 1, MAX_OCTETS, file);
  fclose(file);
  return 1;
}

/* Reads a stream in chunks of the given size; returns 0 on misuse. */
static int read_in_chunks(const unsigned char *octets, size_t size,
                          size_t chunk, struct dots_event *result) {
  struct dots_reader reader;
  size_t at = 0;
  dots_reader_init(&reader);
  while (at < size) {
    size_t end = at + chunk < size ? at + chunk : size;
    while (at < end) {
      struct dots_event event;
      size_t used = dots_reader_feed(&reader, octets + at, end - at, &event);
      if (used > end - at || (used == 0 && event.type == DOTS_EVENT_NONE)) {
        return 0;
      }
      if (event.type == DOTS_EVENT_ERROR) {
        at = size;
        break;
      }
      at += used;
    }
  }
  dots_reader_finish(&reader, result);
  return 1;
}

int main(int argc, char **argv) {
  static unsigned char listed[MAX_OCTETS], stream[MAX_OCTETS];
  size_t listed_size, stream_size, chunk;
  struct expected expected;
  int ok = 1;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <listing.txt> <stream.dots>\n", argv[0]);
    return 2;
  }
  if (!read_listing(argv[1], listed, &listed_size, &expected) ||
      !read_stream(argv[2], stream, &stream_size)) {
    return 1;
  }
  if (stream_size != listed_size || memcmp(stream, listed, stream_size) != 0) {
    fail(argv[2], "differs from its listing");
    ok = 0;
  }

  for (chunk = 1; chunk <= stream_size; ++chunk) {
    struct dots_event result;
    enum dots_code code;
    uint64_t offset = 0;
    if (!read_in_chunks(stream, stream_size, chunk, &result)) {
      fprintf(stderr, "chunk %lu: reader used no input and gave no event\n",
              (unsigned long)chunk);
      return 1;
    }
    code = result.type == DOTS_EVENT_END_OF_STREAM ? DOTS_OK : result.code;
    if (code != DOTS_OK) {
      offset = result.offset;
    }
    if (code != expected.code || offset != expected.offset) {
      fprintf(stderr, "chunk %lu: %s at offset %llu, expected %s at %llu\n",
              (unsigned long)chunk, name(code), (unsigned long long)offset,
              name(expected.code), (unsigned long long)expected.offset);
      ok = 0;
      break;
    }
  }
  return ok ? 0 : 1;
}
