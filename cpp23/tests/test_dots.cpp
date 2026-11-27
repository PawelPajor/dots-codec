// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor

#include "dots.hpp"

#include <catch2/catch_test_macros.hpp>

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

const dots::header text_header = {{0, 1, 0}, 1, {16, 4}, 10};

auto hello() -> dots::frame {
  return {true, {{{0, 0}, {0x39, 0xFF}}}, {{{2, 1}, "$0\xE2\x82\xAC"}}};
}

auto encode(const std::vector<dots::frame> &frames) -> std::vector<std::byte> {
  std::vector<std::byte> out;
  dots::encoder encoder;

  REQUIRE(encoder.start(text_header, std::back_inserter(out)));
  for (const dots::frame &f : frames) {
    REQUIRE(encoder.put(f, std::back_inserter(out)));
  }
  REQUIRE(encoder.finish(std::back_inserter(out)));
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

} // namespace

TEST_CASE("round_trip") {
  const std::vector<dots::frame> frames = {hello(), {false, {}, {}}};
  const std::vector<std::byte> stream = encode(frames);
  dots::decoder decoder;
  const auto decoded = decode(decoder, stream);

  REQUIRE(decoded.has_value());
  CHECK(*decoded == frames);
  CHECK(decoder.finish().has_value());
  CHECK(decoder.stream_header() == text_header);
}

TEST_CASE("octet_by_octet") {
  const std::vector<dots::frame> frames = {hello(), hello()};
  const std::vector<std::byte> stream = encode(frames);
  std::vector<dots::frame> decoded;
  dots::decoder decoder;

  for (const std::byte octet : stream) {
    auto got = decoder.feed(std::span(&octet, 1));

    REQUIRE(got.has_value());
    CHECK(got->used == 1);
    if (got->completed) {
      decoded.push_back(std::move(*got->completed));
    }
  }
  CHECK(decoded == frames);
  CHECK(decoder.finish().has_value());
}

TEST_CASE("truncated") {
  std::vector<std::byte> stream = encode({hello()});
  dots::decoder decoder;

  stream.pop_back(); // end-of-stream
  CHECK(decode(decoder, stream).has_value());
  CHECK(decoder.finish() ==
        std::unexpected(dots::error{dots::code::truncated, stream.size()}));
}

TEST_CASE("one_frame_at_a_time") {
  const std::vector<std::byte> stream = encode({hello(), hello()});
  std::vector<std::byte> trailing = stream;
  dots::decoder decoder;

  trailing.push_back(std::byte{0x42}); // after end-of-stream (STR-2)

  auto first = decoder.feed(trailing);

  REQUIRE(first.has_value());
  CHECK(first->completed == hello());
  CHECK(first->used < stream.size());

  auto second = decoder.feed(std::span(trailing).subspan(first->used));

  REQUIRE(second.has_value());
  CHECK(second->completed == hello());

  const std::span<const std::byte> rest =
      std::span(trailing).subspan(first->used + second->used);
  auto end = decoder.feed(rest);

  REQUIRE(end.has_value());
  CHECK(!end->completed);
  CHECK(end->used == rest.size());
  CHECK(decoder.finish().has_value());
}

TEST_CASE("too_large") {
  // A header of 2^32 columns and 2^32 rows: 2^64 cells.
  const std::uint8_t octets[] = {0x64, 0x6F, 0x74, 0x73, 0x00, 0x01, 0x00,
                                 0x00, 0x80, 0x80, 0x80, 0x80, 0x10, 0x80,
                                 0x80, 0x80, 0x80, 0x10, 0x00, 0xFF};
  dots::decoder decoder;
  dots::encoder encoder;
  std::vector<std::byte> out;

  CHECK(decode(decoder, std::as_bytes(std::span(octets))) ==
        std::unexpected(dots::error{dots::code::too_large, 13}));
  CHECK(encoder.start({{0, 1, 0}, 0, {1ULL << 32, 1ULL << 32}, 0},
                      std::back_inserter(out)) ==
        std::unexpected(dots::error{dots::code::too_large, 0}));
  CHECK(encoder
            .start({{0, 1, 0}, 0, {1ULL << 32, (1ULL << 32) - 1}, 0},
                   std::back_inserter(out))
            .has_value());
}

TEST_CASE("encoder_refusals") {
  std::vector<std::byte> out;
  dots::encoder encoder;
  const auto o = std::back_inserter(out);
  const auto refused = [](const auto &result, dots::code c) -> bool {
    return !result.has_value() && result.error().reason == c;
  };

  CHECK(refused(encoder.put(hello(), o), dots::code::out_of_order));
  CHECK(refused(encoder.start({{1, 0, 0}, 0, {1, 1}, 0}, o),
                dots::code::unsupported_version));
  CHECK(refused(encoder.start({{0, 1, 0}, 2, {1, 1}, 0}, o),
                dots::code::invalid_value));
  CHECK(out.empty());
  CHECK(encoder.start({{0, 1, 0}, 0, {2, 1}, 0}, o).has_value());
  CHECK(refused(encoder.start(text_header, o), dots::code::out_of_order));
  CHECK(refused(encoder.put({false, {}, {{{0, 0}, "A"}}}, o),
                dots::code::unexpected_text));
  CHECK(refused(encoder.put({false, {{{1, 0}, {1, 2}}}, {}}, o),
                dots::code::outside_grid));
  CHECK(encoder.finish(o).has_value());
  CHECK(refused(encoder.finish(o), dots::code::out_of_order));
}

TEST_CASE("composer") {
  // The example of Section 6 of dots-content.md, at cell (3, 0).
  auto created = dots::composer::create(text_header);

  REQUIRE(created.has_value());

  dots::composer &composer = *created;
  const auto at = [&]() -> dots::cell { return composer.cells()[3]; };

  CHECK(
      composer.apply({false, {{{3, 0}, {0xFF}}}, {{{3, 0}, "A"}}}).has_value());
  CHECK(at() == dots::cell{U'A', 0xFF});
  CHECK(composer.apply({false, {}, {{{3, 0}, std::string(1, '\0')}}})
            .has_value());
  CHECK(at() == dots::cell{0, 0xFF});
  CHECK(composer.apply({true, {}, {}}).has_value());
  CHECK(at() == dots::cell{0, 0});
  CHECK(!composer.apply({false, {}, {{{0, 0}, "\xC0"}}}).has_value());
  CHECK(dots::composer::create({{0, 1, 0}, 0, {UINT64_MAX, 1}, 0}) ==
        std::unexpected(dots::error{dots::code::too_large, 0}));
}
