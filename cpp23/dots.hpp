// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor
//
// dots.hpp: reader and writer of DOTS streams, in C++23.
//
// The unit is a frame. A decoder reads frames, one at a time, from octets
// fed to it in pieces of any size, an encoder writes frames to an output
// iterator, and a composer applies frames to the state of the grid. Nothing
// throws: every failure is an error, in a std::expected. Names in dots::detail
// are not part of the interface, and may change at any time.

#ifndef DOTS_HPP
#define DOTS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dots {

// The major and minor version a stream carries (HDR-1).
inline constexpr std::uint64_t spec_major = 0;
inline constexpr std::uint64_t spec_minor = 1;

// Result codes: the non-negative ones with the values of the
// specification, the negative ones for failures it leaves out (ERR-4).
enum class code : std::int8_t {
  out_of_order = -1, // a call the state of the object does not allow
  ok = 0,
  unsupported_version = 1,
  invalid_value = 2,
  unexpected_text = 3,
  outside_grid = 4,
  truncated = 5,
  malformed = 6,
  overflow = 7,
  invalid_text = 8,
  // Not yet in the specification, which is to require that the grid has
  // at most 2^64 - 1 cells: columns * rows does not overflow.
  too_large = 9,
};

// The name of a code, as the specification writes it, or empty if unknown.
constexpr auto code_name(code c) noexcept -> std::string_view {
  switch (c) {
  case code::out_of_order:
    return "OUT_OF_ORDER";
  case code::ok:
    return "OK";
  case code::unsupported_version:
    return "UNSUPPORTED_VERSION";
  case code::invalid_value:
    return "INVALID_VALUE";
  case code::unexpected_text:
    return "UNEXPECTED_TEXT";
  case code::outside_grid:
    return "OUTSIDE_GRID";
  case code::truncated:
    return "TRUNCATED";
  case code::malformed:
    return "MALFORMED";
  case code::overflow:
    return "OVERFLOW";
  case code::invalid_text:
    return "INVALID_TEXT";
  case code::too_large:
    return "TOO_LARGE";
  }
  return {};
}

// 2D coordinates (Section 4.4).
struct coordinate_2d {
  std::uint64_t x;
  std::uint64_t y;

  friend auto operator==(const coordinate_2d &, const coordinate_2d &)
      -> bool = default;
};

// A version (Section 4.3).
struct version {
  std::uint64_t major;
  std::uint64_t minor;
  std::uint64_t update;

  friend auto operator==(const version &, const version &) -> bool = default;
};

// The header of a stream (Section 5). Bit 0 of flags is contains-text.
struct header {
  version dots_version;
  std::uint64_t flags;
  coordinate_2d dimensions; // columns, rows
  std::uint64_t frame_rate;

  friend auto operator==(const header &, const header &) -> bool = default;
};

// A failure: its code, and where. For a decoder, the offset in the stream
// (Section 8); for an encoder, the number of octets it has written; for a
// composer, which has no stream, always 0.
struct error {
  code reason;
  std::uint64_t offset;

  friend auto operator==(const error &, const error &) -> bool = default;
};

// A run of the block layer: one block per octet.
struct block_run {
  coordinate_2d position;
  std::vector<std::uint8_t> blocks;

  friend auto operator==(const block_run &, const block_run &)
      -> bool = default;
};

// A run of the text layer: UTF-8.
struct text_run {
  coordinate_2d position;
  std::string text;

  friend auto operator==(const text_run &, const text_run &) -> bool = default;
};

// A frame, as the stream holds it: what changes, not the state.
struct frame {
  bool clear;
  std::vector<block_run> block_layer;
  std::vector<text_run> text_layer;

  friend auto operator==(const frame &, const frame &) -> bool = default;
};

// A cell of the state.
struct cell {
  char32_t character; // 0 if it has none
  std::uint8_t block;

  friend auto operator==(const cell &, const cell &) -> bool = default;
};

// The Braille pattern of a block (Section 5 of dots-content.md).
constexpr auto braille(std::uint8_t block) -> char32_t {
  constexpr std::uint8_t added[] = {0x01, 0x08, 0x02, 0x10,
                                    0x04, 0x20, 0x40, 0x80};
  char32_t pattern = 0x2800;

  for (int bit = 0; bit < 8; ++bit) {
    if ((block >> bit) & 1) {
      pattern += added[bit];
    }
  }
  return pattern;
}

namespace detail {

inline constexpr std::uint8_t token_clear = 0x01;
inline constexpr std::uint8_t token_start_of_run = 0x02;
inline constexpr std::uint8_t token_end_of_text = 0xFD;
inline constexpr std::uint8_t token_end_of_blocks = 0xFE;
inline constexpr std::uint8_t token_end_of_stream = 0xFF;
inline constexpr std::uint8_t fourcc[] = {0x64, 0x6F, 0x74, 0x73};
inline constexpr std::uint64_t contains_text = 1;

// The result of giving an octet to an incremental parser.
enum class step : std::uint8_t { more, done, invalid };

// A leb128 (Section 4.1), octet by octet. Overflow is known only once the
// leb128 ends: an octet sequence that never matches has no value (INT-2).
struct leb128_reader {
  std::uint64_t value = 0;
  unsigned shift = 0;
  bool overflow = false;

  constexpr auto put(std::uint8_t octet) -> step {
    const std::uint64_t payload = octet & 0x7F;

    if (shift < 63) {
      value |= payload << shift;
    } else if (shift == 63) {
      overflow = overflow || payload > 1;
      value |= payload << shift;
    } else {
      overflow = overflow || payload != 0;
    }
    if (octet & 0x80) {
      shift = shift < 70 ? shift + 7 : shift;
      return step::more;
    }
    return shift != 0 && octet == 0 ? step::invalid : step::done;
  }
};

// Well-formed UTF-8 (RFC 3629), octet by octet.
struct utf8_reader {
  char32_t value = 0;
  int needed = 0; // continuation octets still to come
  std::uint8_t lower = 0x80;
  std::uint8_t upper = 0xBF;

  constexpr auto pending() const -> bool { return needed != 0; }

  constexpr auto put(std::uint8_t octet) -> step {
    if (needed == 0) {
      return lead(octet);
    }
    if (octet < lower || octet > upper) {
      return step::invalid;
    }
    value = (value << 6) | (octet & 0x3F);
    lower = 0x80;
    upper = 0xBF;
    --needed;
    return needed == 0 ? step::done : step::more;
  }

private:
  constexpr auto begin(std::uint8_t bits, int continuations) -> step {
    value = bits;
    needed = continuations;
    return step::more;
  }

  constexpr auto lead(std::uint8_t octet) -> step {
    lower = 0x80;
    upper = 0xBF;
    if (octet < 0x80) {
      value = octet;
      return step::done;
    }
    if (octet >= 0xC2 && octet <= 0xDF) {
      return begin(octet & 0x1F, 1);
    }
    if (octet >= 0xE0 && octet <= 0xEF) {
      lower = octet == 0xE0 ? 0xA0 : 0x80;
      upper = octet == 0xED ? 0x9F : 0xBF;
      return begin(octet & 0x0F, 2);
    }
    if (octet >= 0xF0 && octet <= 0xF4) {
      lower = octet == 0xF0 ? 0x90 : 0x80;
      upper = octet == 0xF4 ? 0x8F : 0xBF;
      return begin(octet & 0x07, 3);
    }
    return step::invalid;
  }
};

// The characters of well-formed UTF-8, each given to f in order, or false.
template <class F> constexpr auto each_character(std::string_view text, F f) {
  utf8_reader reader;

  for (const char c : text) {
    const step s = reader.put(static_cast<std::uint8_t>(c));

    if (s == step::invalid) {
      return false;
    }
    if (s == step::done) {
      f(reader.value);
    }
  }
  return !reader.pending();
}

// The stream requirements of a header.
constexpr auto check(const header &h) -> code {
  if (h.dots_version.major != spec_major ||
      h.dots_version.minor != spec_minor) {
    return code::unsupported_version;
  }
  if (h.dimensions.x == 0 || h.dimensions.y == 0 ||
      (h.flags & ~contains_text) != 0) {
    return code::invalid_value;
  }
  if (h.dimensions.x > (UINT64_MAX / h.dimensions.y)) {
    return code::too_large;
  }
  return code::ok;
}

// The stream requirements of a frame of a stream with the header h.
constexpr auto check(const header &h, const frame &f) -> code {
  const auto inside = [&](coordinate_2d p) -> bool {
    return p.x < h.dimensions.x && p.y < h.dimensions.y;
  };

  for (const block_run &run : f.block_layer) {
    if (!inside(run.position) ||
        run.blocks.size() > h.dimensions.x - run.position.x) {
      return code::outside_grid;
    }
  }
  if (!f.text_layer.empty() && (h.flags & contains_text) == 0) {
    return code::unexpected_text;
  }
  for (const text_run &run : f.text_layer) {
    std::uint64_t characters = 0;

    if (!inside(run.position)) {
      return code::outside_grid;
    }
    if (!each_character(run.text, [&](char32_t) -> void { ++characters; })) {
      return code::invalid_text;
    }
    if (characters > h.dimensions.x - run.position.x) {
      return code::outside_grid;
    }
  }
  return code::ok;
}

} // namespace detail

// What a call of decoder::feed read.
struct fed {
  std::size_t used;               // octets of the input read
  std::optional<frame> completed; // the frame that ended at the last of them
};

// Reads a stream fed in pieces of any size, in order.
class decoder {
public:
  // Reads input up to the end of the next frame, and gives that frame, or
  // all input and no frame. The rest of the input, input.subspan(used), is
  // for the next call. After end-of-stream, uses all input and ignores it
  // (STR-2); after an error, gives it again.
  auto feed(std::span<const std::byte> input) -> std::expected<fed, error> {
    fed result{0, std::nullopt};

    for (const std::byte octet : input) {
      if (state_ == state::ended || state_ == state::failed) {
        break;
      }
      step(std::to_integer<std::uint8_t>(octet), result.completed);
      ++offset_;
      ++result.used;
      if (result.completed) {
        return result;
      }
    }
    if (failure_) {
      return std::unexpected(*failure_);
    }
    result.used = input.size();
    return result;
  }

  // Ends the input: TRUNCATED if the stream has not ended.
  auto finish() -> std::expected<void, error> {
    if (state_ != state::ended && state_ != state::failed) {
      fail(code::truncated, offset_);
    }
    if (failure_) {
      return std::unexpected(*failure_);
    }
    return {};
  }

  // The header, once read.
  auto stream_header() const noexcept -> const std::optional<header> & {
    return header_;
  }

private:
  enum class state : std::uint8_t {
    fourcc,
    header_field,
    frame_start,
    block_layer,
    text_layer,
    run_x,
    run_y,
    run_count,
    run_data,
    ended,
    failed,
  };

  state state_ = state::fourcc;
  std::uint64_t offset_ = 0; // of the octet being read
  std::optional<error> failure_;
  std::optional<header> header_;
  header fields_{};
  int field_ = 0; // the next field of the header, in stream order
  detail::leb128_reader leb128_;
  std::uint64_t leb128_offset_ = 0;
  frame frame_{};
  bool text_ = false; // the run is in the text layer
  coordinate_2d position_{};
  std::uint64_t remaining_ = 0; // octets of the run still to come
  std::vector<std::uint8_t> blocks_;
  std::string text_data_;
  detail::utf8_reader utf8_;
  std::uint64_t sequence_offset_ = 0; // of the UTF-8 sequence being read
  std::uint64_t characters_ = 0;      // of the text run so far

  auto fail(code c, std::uint64_t offset) -> void {
    failure_ = error{c, offset};
    state_ = state::failed;
  }

  // A leb128 octet: its value once it ends, or nothing.
  auto leb128(std::uint8_t octet) -> std::optional<std::uint64_t> {
    if (leb128_.shift == 0) {
      leb128_offset_ = offset_;
    }

    const detail::step s = leb128_.put(octet);

    if (s == detail::step::more) {
      return std::nullopt;
    }

    const detail::leb128_reader done = std::exchange(leb128_, {});

    if (s == detail::step::invalid) {
      fail(code::malformed, offset_);
      return std::nullopt;
    }
    if (done.overflow) {
      fail(code::overflow, leb128_offset_);
      return std::nullopt;
    }
    return done.value;
  }

  auto step(std::uint8_t octet, std::optional<frame> &completed) -> void {
    switch (state_) {
    case state::fourcc:
      if (octet != detail::fourcc[offset_]) {
        fail(code::malformed, offset_);
      } else if (offset_ == 3) {
        state_ = state::header_field;
      }
      return;
    case state::header_field:
      if (const auto value = leb128(octet)) {
        header_field(*value);
      }
      return;
    case state::frame_start:
      frame_start(octet, completed);
      return;
    case state::block_layer:
      layer(octet, detail::token_end_of_blocks, completed);
      return;
    case state::text_layer:
      layer(octet, detail::token_end_of_text, completed);
      return;
    case state::run_x:
    case state::run_y:
    case state::run_count:
      if (const auto value = leb128(octet)) {
        run_field(*value);
      }
      return;
    case state::run_data:
      run_data(octet);
      return;
    case state::ended:
    case state::failed:
      return;
    }
  }

  auto header_field(std::uint64_t value) -> void {
    switch (field_++) {
    case 0:
      fields_.dots_version.major = value;
      if (value != spec_major) {
        fail(code::unsupported_version, leb128_offset_);
      }
      return;
    case 1:
      fields_.dots_version.minor = value;
      if (value != spec_minor) {
        fail(code::unsupported_version, leb128_offset_);
      }
      return;
    case 2:
      fields_.dots_version.update = value;
      return;
    case 3:
      fields_.flags = value;
      if ((value & ~detail::contains_text) != 0) {
        fail(code::invalid_value, leb128_offset_);
      }
      return;
    case 4:
      fields_.dimensions.x = value;
      if (value == 0) {
        fail(code::invalid_value, leb128_offset_);
      }
      return;
    case 5:
      fields_.dimensions.y = value;
      if (value == 0) {
        fail(code::invalid_value, leb128_offset_);
      } else if (fields_.dimensions.x > (UINT64_MAX / value)) {
        fail(code::too_large, leb128_offset_);
      }
      return;
    default:
      fields_.frame_rate = value;
      header_ = fields_;
      state_ = state::frame_start;
      return;
    }
  }

  auto frame_start(std::uint8_t octet, std::optional<frame> &completed)
      -> void {
    switch (octet) {
    case detail::token_end_of_stream:
      state_ = state::ended;
      return;
    case detail::token_clear:
      frame_ = frame{true, {}, {}};
      state_ = state::block_layer;
      return;
    case detail::token_start_of_run:
    case detail::token_end_of_blocks:
      frame_ = frame{false, {}, {}};
      state_ = state::block_layer;
      layer(octet, detail::token_end_of_blocks, completed);
      return;
    default:
      fail(code::malformed, offset_);
      return;
    }
  }

  // An octet where a run or the end of the layer comes.
  auto layer(std::uint8_t octet, std::uint8_t end,
             std::optional<frame> &completed) -> void {
    if (octet == detail::token_start_of_run) {
      text_ = state_ == state::text_layer;
      if (text_ && (header_->flags & detail::contains_text) == 0) {
        fail(code::unexpected_text, offset_);
        return;
      }
      state_ = state::run_x;
    } else if (octet != end) {
      fail(code::malformed, offset_);
    } else if (state_ == state::block_layer) {
      state_ = state::text_layer;
    } else {
      completed = std::move(frame_);
      frame_ = {};
      state_ = state::frame_start;
    }
  }

  auto run_field(std::uint64_t value) -> void {
    switch (state_) {
    case state::run_x:
      position_.x = value;
      if (value >= header_->dimensions.x) {
        fail(code::outside_grid, leb128_offset_);
        return;
      }
      state_ = state::run_y;
      return;
    case state::run_y:
      position_.y = value;
      if (value >= header_->dimensions.y) {
        fail(code::outside_grid, leb128_offset_);
        return;
      }
      state_ = state::run_count;
      return;
    default:
      remaining_ = value;
      characters_ = 0;
      state_ = state::run_data;
      if (remaining_ == 0) {
        end_run();
      }
      return;
    }
  }

  auto run_data(std::uint8_t octet) -> void {
    const std::uint64_t room = header_->dimensions.x - position_.x;

    if (!text_) {
      if (blocks_.size() >= room) {
        fail(code::outside_grid, offset_);
        return;
      }
      blocks_.push_back(octet);
    } else {
      if (!utf8_.pending()) {
        sequence_offset_ = offset_;
      }

      const detail::step s = utf8_.put(octet);

      if (s == detail::step::invalid ||
          (remaining_ == 1 && s == detail::step::more)) {
        fail(code::invalid_text, sequence_offset_);
        return;
      }
      if (s == detail::step::done && characters_++ >= room) {
        fail(code::outside_grid, sequence_offset_);
        return;
      }
      text_data_.push_back(static_cast<char>(octet));
    }
    if (--remaining_ == 0) {
      end_run();
    }
  }

  auto end_run() -> void {
    if (text_) {
      frame_.text_layer.push_back(text_run{position_, std::move(text_data_)});
      text_data_.clear();
      state_ = state::text_layer;
    } else {
      frame_.block_layer.push_back(block_run{position_, std::move(blocks_)});
      blocks_.clear();
      state_ = state::block_layer;
    }
  }
};

// Writes a stream: start, then put for each frame, then finish. Each call
// checks its input whole before writing, so a refused call writes nothing
// and changes nothing.
class encoder {
public:
  // Writes the header.
  template <std::output_iterator<std::byte> O>
  auto start(const header &h, O out) -> std::expected<O, error> {
    if (state_ != state::initial) {
      return refuse(code::out_of_order);
    }
    if (const code c = detail::check(h); c != code::ok) {
      return refuse(c);
    }
    header_ = h;
    state_ = state::started;
    for (const std::uint8_t octet : detail::fourcc) {
      out = write(octet, std::move(out));
    }
    out = write_leb128(h.dots_version.major, std::move(out));
    out = write_leb128(h.dots_version.minor, std::move(out));
    out = write_leb128(h.dots_version.update, std::move(out));
    out = write_leb128(h.flags, std::move(out));
    out = write_leb128(h.dimensions.x, std::move(out));
    out = write_leb128(h.dimensions.y, std::move(out));
    return write_leb128(h.frame_rate, std::move(out));
  }

  // Writes a frame.
  template <std::output_iterator<std::byte> O>
  auto put(const frame &f, O out) -> std::expected<O, error> {
    if (state_ != state::started) {
      return refuse(code::out_of_order);
    }
    if (const code c = detail::check(header_, f); c != code::ok) {
      return refuse(c);
    }
    if (f.clear) {
      out = write(detail::token_clear, std::move(out));
    }
    for (const block_run &run : f.block_layer) {
      out = write_run(run.position, run.blocks, std::move(out));
    }
    out = write(detail::token_end_of_blocks, std::move(out));
    for (const text_run &run : f.text_layer) {
      out = write_run(run.position, run.text, std::move(out));
    }
    return write(detail::token_end_of_text, std::move(out));
  }

  // Writes end-of-stream.
  template <std::output_iterator<std::byte> O>
  auto finish(O out) -> std::expected<O, error> {
    if (state_ != state::started) {
      return refuse(code::out_of_order);
    }
    state_ = state::finished;
    return write(detail::token_end_of_stream, std::move(out));
  }

private:
  enum class state : std::uint8_t { initial, started, finished };

  state state_ = state::initial;
  header header_{};
  std::uint64_t written_ = 0;

  auto refuse(code c) const -> std::unexpected<error> {
    return std::unexpected(error{c, written_});
  }

  template <class O> auto write(std::uint8_t octet, O out) -> O {
    *out = std::byte{octet};
    ++out;
    ++written_;
    return out;
  }

  template <class O> auto write_leb128(std::uint64_t value, O out) -> O {
    while (value >= 0x80) {
      out = write(static_cast<std::uint8_t>(value | 0x80), std::move(out));
      value >>= 7;
    }
    return write(static_cast<std::uint8_t>(value), std::move(out));
  }

  template <class R, class O>
  auto write_run(coordinate_2d position, const R &data, O out) -> O {
    out = write(detail::token_start_of_run, std::move(out));
    out = write_leb128(position.x, std::move(out));
    out = write_leb128(position.y, std::move(out));
    out = write_leb128(data.size(), std::move(out));
    for (const auto octet : data) {
      out = write(static_cast<std::uint8_t>(octet), std::move(out));
    }
    return out;
  }
};

// The state of the grid (Section 6 of dots-content.md), frame by frame.
// Move-only: it owns the cells.
class composer {
public:
  // The initial state: TOO_LARGE if the cells do not fit in memory, or the
  // error of a header that breaks a stream requirement.
  static auto create(const header &h) -> std::expected<composer, error> {
    if (const code c = detail::check(h); c != code::ok) {
      return std::unexpected(error{c, 0});
    }

    // Cannot overflow: detail::check has returned too_large if it would.
    const std::uint64_t count = h.dimensions.x * h.dimensions.y;

    if (count > SIZE_MAX / sizeof(cell)) {
      return std::unexpected(error{code::too_large, 0});
    }

    const auto size = static_cast<std::size_t>(count);
    std::unique_ptr<cell[]> cells(new (std::nothrow) cell[size]());

    if (!cells) {
      return std::unexpected(error{code::too_large, 0});
    }
    return composer(h, std::move(cells), size);
  }

  // Applies a frame. A refused frame changes nothing.
  auto apply(const frame &f) -> std::expected<void, error> {
    if (const code c = detail::check(header_, f); c != code::ok) {
      return std::unexpected(error{c, 0});
    }
    if (f.clear) {
      std::ranges::fill(writable(), cell{});
    }
    for (const block_run &run : f.block_layer) {
      std::size_t i = index(run.position);

      for (const std::uint8_t block : run.blocks) {
        cells_[i++].block = block;
      }
    }
    for (const text_run &run : f.text_layer) {
      std::size_t i = index(run.position);

      detail::each_character(
          run.text, [&](char32_t c) -> void { cells_[i++].character = c; });
    }
    return {};
  }

  // The cells, cell (x, y) at index y * columns + x.
  auto cells() const noexcept -> std::span<const cell> {
    return {cells_.get(), size_};
  }

private:
  header header_;
  std::unique_ptr<cell[]> cells_;
  std::size_t size_;

  composer(const header &h, std::unique_ptr<cell[]> cells, std::size_t size)
      : header_{h}, cells_{std::move(cells)}, size_{size} {}

  auto writable() noexcept -> std::span<cell> { return {cells_.get(), size_}; }

  auto index(coordinate_2d p) const -> std::size_t {
    return static_cast<std::size_t>(p.y * header_.dimensions.x + p.x);
  }
};

} // namespace dots

#endif // DOTS_HPP
