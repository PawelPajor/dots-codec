// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor
//
// One stream: test_acceptance <dir> <name>, of the validation suite as
// accept/name or reject/name, or of the examples. Read whole and octet by
// octet. A reject case gives the code and offset of its .meta. Any other
// stream is accepted, composed and compared with its .surface if it has
// one, and written again: octet for octet the same stream, up to its
// end-of-stream (STR-2).

#include "dots.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

auto read(const std::filesystem::path &path) -> std::string {
  std::ifstream in(path, std::ios::binary);

  return {std::istreambuf_iterator<char>(in), {}};
}

// The value of a top-level `key = value` line of a .meta, unquoted.
auto meta_value(const std::string &meta, const std::string &key)
    -> std::string {
  const std::string start = key + " = ";
  const std::size_t at = meta.find("\n" + start);

  if (at == std::string::npos) {
    return {};
  }

  const std::size_t from = at + 1 + start.size();
  std::string value = meta.substr(from, meta.find('\n', from) - from);

  if (value.ends_with('\r')) { // TOML allows CRLF line ends
    value.pop_back();
  }
  if (value.size() >= 2 && value.front() == '"') {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

auto utf8(char32_t c, std::string &out) -> void {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

struct outcome {
  std::optional<dots::header> header;
  std::vector<dots::frame> frames;
  dots::error result;

  auto operator==(const outcome &) const -> bool = default;
};

// Decodes the stream in pieces of the given size.
auto decode(std::span<const std::byte> stream, std::size_t piece) -> outcome {
  dots::decoder decoder;
  outcome o{std::nullopt, {}, {dots::code::ok, 0}};

  for (std::size_t at = 0; at < stream.size(); at += piece) {
    std::span<const std::byte> input =
        stream.subspan(at, std::min(piece, stream.size() - at));

    while (!input.empty()) {
      auto got = decoder.feed(input);

      if (!got) {
        o.result = got.error();
        break;
      }
      if (got->completed) {
        o.frames.push_back(std::move(*got->completed));
      }
      input = input.subspan(got->used);
    }
    if (o.result.reason != dots::code::ok) {
      break;
    }
  }
  if (o.result.reason == dots::code::ok) {
    if (auto end = decoder.finish(); !end) {
      o.result = end.error();
    }
  }
  o.header = decoder.stream_header();
  return o;
}

auto surface(const outcome &o) -> std::string {
  std::string out;
  auto composer = dots::composer::create(*o.header);

  if (!composer) {
    return "composer refused the header";
  }
  for (const dots::frame &f : o.frames) {
    if (!composer->apply(f)) {
      return "composer refused a frame";
    }
    for (const dots::cell &c : composer->cells()) {
      utf8(c.character != 0 ? c.character : dots::braille(c.block), out);
    }
  }
  return out;
}

auto encode(const outcome &o) -> std::vector<std::byte> {
  std::vector<std::byte> out;
  dots::encoder encoder;

  if (!encoder.start(*o.header, std::back_inserter(out))) {
    return {};
  }
  for (const dots::frame &f : o.frames) {
    if (!encoder.put(f, std::back_inserter(out))) {
      return {};
    }
  }
  (void)encoder.finish(std::back_inserter(out));
  return out;
}

auto fail(const std::string &what) -> int {
  std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  return 1;
}

} // namespace

auto main(int argc, char **argv) -> int {
  if (argc != 3) {
    return fail("usage: test_acceptance <suite> <case>");
  }

  const std::filesystem::path base = std::filesystem::path(argv[1]) / argv[2];
  const std::string bytes = read(base.string() + ".dots");
  const std::span<const std::byte> stream = std::as_bytes(std::span(bytes));
  const std::string meta = read(base.string() + ".meta");
  const outcome whole = decode(stream, stream.size() + 1);

  if (decode(stream, 1) != whole) {
    return fail("octet by octet differs from whole");
  }
  if (std::string(argv[2]).starts_with("reject/")) {
    const std::string got = std::string(dots::code_name(whole.result.reason)) +
                            " at " + std::to_string(whole.result.offset);
    const std::string want =
        meta_value(meta, "code") + " at " + meta_value(meta, "offset");

    return got == want ? 0 : fail(got + ", want " + want);
  }
  if (whole.result.reason != dots::code::ok) {
    return fail(std::string(dots::code_name(whole.result.reason)) + " at " +
                std::to_string(whole.result.offset));
  }
  if (std::filesystem::exists(base.string() + ".surface") &&
      surface(whole) != read(base.string() + ".surface")) {
    return fail("surface differs");
  }

  // A complete stream that the original starts with is the original up to
  // its end-of-stream: the octets after it are not part of the stream.
  const std::vector<std::byte> again = encode(whole);

  if (again.empty() || again.size() > stream.size() ||
      !std::ranges::equal(again, stream.first(again.size()))) {
    return fail("written again, the octets differ");
  }
  return 0;
}
