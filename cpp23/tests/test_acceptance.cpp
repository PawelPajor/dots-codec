// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor
//
// One stream: test_acceptance <dir> <name>, of the validation suite as
// accept/name or reject/name, or of the examples. Read whole and octet by
// octet. A reject case gives the code and offset of its .meta. Any other
// stream is accepted, with the header of its .meta if it has one, composed
// and compared with its .surface if it has one, and written again: octet
// for octet the same stream, up to its end-of-stream (STR-2).

#include "dots.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

auto read(const std::filesystem::path &path) -> std::string {
  std::ifstream in(path, std::ios::binary);

  return {std::istreambuf_iterator<char>(in), {}};
}

// A string of decimal digits, as a std::uint64_t.
auto decimal(std::string_view text) -> std::optional<std::uint64_t> {
  const char *const end = text.data() + text.size();
  std::uint64_t value = 0;
  const auto [at, ec] = std::from_chars(text.data(), end, value);

  if (ec != std::errc{} || at != end) {
    return std::nullopt;
  }
  return value;
}

// dots-version: "major.minor.update".
auto version(std::string_view text) -> std::optional<dots::version> {
  std::uint64_t parts[3] = {};

  for (int i = 0; i < 3; ++i) {
    const std::size_t dot = i < 2 ? text.find('.') : text.size();

    if (dot == std::string_view::npos) {
      return std::nullopt;
    }

    const auto part = decimal(text.substr(0, dot));

    if (!part) {
      return std::nullopt;
    }
    parts[i] = *part;
    text.remove_prefix(i < 2 ? dot + 1 : dot);
  }
  return dots::version{parts[0], parts[1], parts[2]};
}

// The [header] of an accept case's .meta.
auto meta_header(const toml::table &meta) -> std::optional<dots::header> {
  const toml::node_view<const toml::node> h = meta["header"];
  const auto dots_version = h["dots-version"].value_exact<std::string>();
  const auto contains_text = h["contains-text"].value_exact<bool>();
  const auto columns = h["columns"].value_exact<std::int64_t>();
  const auto rows = h["rows"].value_exact<std::int64_t>();
  const auto frame_rate = h["frame-rate"].value_exact<std::string>();

  if (!dots_version || !contains_text || !columns || !rows || !frame_rate ||
      *columns < 0 || *rows < 0) {
    return std::nullopt;
  }

  const auto v = version(*dots_version);
  const auto rate = decimal(*frame_rate);

  if (!v || !rate) {
    return std::nullopt;
  }
  return dots::header{
      *v,
      *contains_text ? 1U : 0U,
      {static_cast<std::uint64_t>(*columns), static_cast<std::uint64_t>(*rows)},
      *rate};
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
  const std::string name = argv[2];
  const bool accept = name.starts_with("accept/");
  const bool reject = name.starts_with("reject/");
  toml::table meta;

  if (accept || reject) {
    try {
      meta = toml::parse_file(base.string() + ".meta");
    } catch (const toml::parse_error &e) {
      return fail(".meta: " + std::string(e.description()));
    }
  }

  const std::string bytes = read(base.string() + ".dots");
  const std::span<const std::byte> stream = std::as_bytes(std::span(bytes));
  const outcome whole = decode(stream, stream.size() + 1);

  if (decode(stream, 1) != whole) {
    return fail("octet by octet differs from whole");
  }
  if (reject) {
    const std::string got = std::string(dots::code_name(whole.result.reason)) +
                            " at " + std::to_string(whole.result.offset);
    const auto code = meta["code"].value_exact<std::string>();
    const auto offset = meta["offset"].value_exact<std::int64_t>();

    if (!code || !offset || *offset < 0 || meta.contains("header")) {
      return fail("a reject case's .meta without a well-formed code and "
                  "offset");
    }

    const std::string want = *code + " at " + std::to_string(*offset);

    return got == want ? 0 : fail(got + ", want " + want);
  }
  if (whole.result.reason != dots::code::ok) {
    return fail(std::string(dots::code_name(whole.result.reason)) + " at " +
                std::to_string(whole.result.offset));
  }
  if (accept) {
    const auto want = meta_header(meta);

    if (!want || meta.contains("code")) {
      return fail("an accept case's .meta without a well-formed [header]");
    }
    if (whole.header != want) {
      return fail("header differs");
    }
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
