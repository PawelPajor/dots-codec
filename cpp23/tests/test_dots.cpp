// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor

#include "dots.hpp"

#include <cstdio>
#include <iterator>
#include <vector>

static_assert(dots::code_name(dots::code::ok) == "OK");
static_assert(dots::code_name(dots::code::invalid_text) == "INVALID_TEXT");
static_assert(dots::code_name(dots::code::out_of_order) == "OUT_OF_ORDER");
static_assert(dots::code_name(dots::code::too_large) == "TOO_LARGE");
static_assert(dots::code_name(static_cast<dots::code>(10)).empty());
static_assert(dots::braille(0x00) == U'\u2800');
static_assert(dots::braille(0x39) == U'\u2835'); // Section 5 of dots-content
static_assert(dots::braille(0xFF) == U'\u28FF');

namespace {

int failures = 0;

auto check(bool ok, const char *what) -> void {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

const dots::header text_header = {{0, 1, 0}, 1, {16, 4}, 10};

auto hello() -> dots::frame {
  return {true, {{{0, 0}, {0x39, 0xFF}}}, {{{2, 1}, "$0\xE2\x82\xAC"}}};
}

auto encode(const std::vector<dots::frame> &frames) -> std::vector<std::byte> {
  std::vector<std::byte> out;
  dots::encoder encoder;

  check(encoder.start(text_header, std::back_inserter(out)).has_value(),
        "start");
  for (const dots::frame &f : frames) {
    check(encoder.put(f, std::back_inserter(out)).has_value(), "put");
  }
  check(encoder.finish(std::back_inserter(out)).has_value(), "finish");
  return out;
}

// Feeds input whole, one call after another, and gives each frame.
auto decode(dots::decoder &decoder, std::span<const std::byte> input)
    -> std::expected<std::vector<dots::frame>, dots::error> {
  std::vector<dots::frame> frames;

  while (!input.empty()) {
    auto got = decoder.feed(input);

    if (!got) {
      return std::unexpected(got.error());
    }
    if (got->completed) {
      frames.push_back(std::move(*got->completed));
    }
    input = input.subspan(got->used);
  }
  return frames;
}

auto round_trip() -> void {
  const std::vector<dots::frame> frames = {hello(), {false, {}, {}}};
  const std::vector<std::byte> stream = encode(frames);
  dots::decoder decoder;
  const auto decoded = decode(decoder, stream);

  check(decoded.has_value() && *decoded == frames, "round trip: frames");
  check(decoder.finish().has_value(), "round trip: finish");
  check(decoder.stream_header() == text_header, "round trip: header");
}

auto octet_by_octet() -> void {
  const std::vector<dots::frame> frames = {hello(), hello()};
  const std::vector<std::byte> stream = encode(frames);
  std::vector<dots::frame> decoded;
  dots::decoder decoder;

  for (const std::byte octet : stream) {
    auto got = decoder.feed(std::span(&octet, 1));

    check(got.has_value() && got->used == 1, "octet by octet: feed");
    if (got && got->completed) {
      decoded.push_back(std::move(*got->completed));
    }
  }
  check(decoded == frames, "octet by octet: frames");
  check(decoder.finish().has_value(), "octet by octet: finish");
}

auto truncated() -> void {
  std::vector<std::byte> stream = encode({hello()});
  dots::decoder decoder;

  stream.pop_back(); // end-of-stream
  check(decode(decoder, stream).has_value(), "truncated: feed");
  check(decoder.finish() ==
            std::unexpected(dots::error{dots::code::truncated, stream.size()}),
        "truncated: finish");
}

auto one_frame_at_a_time() -> void {
  const std::vector<std::byte> stream = encode({hello(), hello()});
  std::vector<std::byte> trailing = stream;
  dots::decoder decoder;

  trailing.push_back(std::byte{0x42}); // after end-of-stream (STR-2)

  auto first = decoder.feed(trailing);

  check(first && first->completed == hello() && first->used < stream.size(),
        "first frame, the rest unread");

  auto second = decoder.feed(std::span(trailing).subspan(first->used));

  check(second && second->completed == hello(), "second frame");

  const std::span<const std::byte> rest =
      std::span(trailing).subspan(first->used + second->used);
  auto end = decoder.feed(rest);

  check(end && !end->completed && end->used == rest.size(),
        "end-of-stream and what follows, all used");
  check(decoder.finish().has_value(), "one at a time: finish");
}

auto too_large() -> void {
  // A header of 2^32 columns and 2^32 rows: 2^64 cells.
  const std::uint8_t octets[] = {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00,
                                 0x00, 0x80, 0x80, 0x80, 0x80, 0x10, 0x80,
                                 0x80, 0x80, 0x80, 0x10, 0x00, 0xFF};
  dots::decoder decoder;
  dots::encoder encoder;
  std::vector<std::byte> out;

  check(decode(decoder, std::as_bytes(std::span(octets))) ==
            std::unexpected(dots::error{dots::code::too_large, 13}),
        "too large: decoder, at rows");
  check(encoder.start({{0, 1, 0}, 0, {1ULL << 32, 1ULL << 32}, 0},
                      std::back_inserter(out)) ==
            std::unexpected(dots::error{dots::code::too_large, 0}),
        "too large: encoder");
  check(encoder
            .start({{0, 1, 0}, 0, {1ULL << 32, (1ULL << 32) - 1}, 0},
                   std::back_inserter(out))
            .has_value(),
        "2^64 - 2^32 cells: encoder");
}

auto encoder_refusals() -> void {
  std::vector<std::byte> out;
  dots::encoder encoder;
  const auto o = std::back_inserter(out);
  const auto refused = [](const auto &result, dots::code c) -> bool {
    return !result.has_value() && result.error().reason == c;
  };

  check(refused(encoder.put(hello(), o), dots::code::out_of_order),
        "put before start");
  check(refused(encoder.start({{1, 0, 0}, 0, {1, 1}, 0}, o),
                dots::code::unsupported_version),
        "start: version");
  check(refused(encoder.start({{0, 1, 0}, 2, {1, 1}, 0}, o),
                dots::code::invalid_value),
        "start: flags");
  check(out.empty(), "refusals write nothing");
  check(encoder.start({{0, 1, 0}, 0, {2, 1}, 0}, o).has_value(), "start");
  check(refused(encoder.start(text_header, o), dots::code::out_of_order),
        "start twice");
  check(refused(encoder.put({false, {}, {{{0, 0}, "A"}}}, o),
                dots::code::unexpected_text),
        "put: text without contains-text");
  check(refused(encoder.put({false, {{{1, 0}, {1, 2}}}, {}}, o),
                dots::code::outside_grid),
        "put: run past the row");
  check(encoder.finish(o).has_value(), "finish");
  check(refused(encoder.finish(o), dots::code::out_of_order), "finish twice");
}

auto composer() -> void {
  // The example of Section 6 of dots-content.md, at cell (3, 0).
  auto created = dots::composer::create(text_header);

  check(created.has_value(), "create");
  if (!created) {
    return;
  }

  dots::composer &composer = *created;
  const auto at = [&]() -> dots::cell { return composer.cells()[3]; };

  check(
      composer.apply({false, {{{3, 0}, {0xFF}}}, {{{3, 0}, "A"}}}).has_value(),
      "frame 1");
  check(at() == dots::cell{U'A', 0xFF}, "frame 1: state");
  check(
      composer.apply({false, {}, {{{3, 0}, std::string(1, '\0')}}}).has_value(),
      "frame 2");
  check(at() == dots::cell{0, 0xFF}, "frame 2: eraser");
  check(composer.apply({true, {}, {}}).has_value(), "frame 3");
  check(at() == dots::cell{0, 0}, "frame 3: clear");
  check(!composer.apply({false, {}, {{{0, 0}, "\xC0"}}}).has_value(),
        "refuses invalid text");
  check(dots::composer::create({{0, 1, 0}, 0, {UINT64_MAX, 1}, 0}) ==
            std::unexpected(dots::error{dots::code::too_large, 0}),
        "create: cells beyond memory");
}

} // namespace

auto main() -> int {
  round_trip();
  octet_by_octet();
  truncated();
  one_frame_at_a_time();
  too_large();
  encoder_refusals();
  composer();
  return failures == 0 ? 0 : 1;
}
