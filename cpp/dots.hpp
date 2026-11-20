// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor
//
// dots.hpp: reader and writer of DOTS streams, in C++23.

#ifndef DOTS_HPP
#define DOTS_HPP

#include <cstdint>
#include <string_view>

namespace dots {

// The version of the specification this header implements.
inline constexpr std::uint64_t spec_major = 0;
inline constexpr std::uint64_t spec_minor = 1;
inline constexpr std::uint64_t spec_update = 1;

// Result codes, with the values of the specification.
enum class code : std::uint8_t {
  ok = 0,
  unsupported_version = 1,
  invalid_value = 2,
  unexpected_text = 3,
  outside_grid = 4,
  truncated = 5,
  malformed = 6,
  overflow = 7,
  invalid_text = 8,
};

// The name of a code, as the specification writes it, or empty if unknown.
constexpr auto code_name(code c) noexcept -> std::string_view {
  switch (c) {
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
  }
  return {};
}

} // namespace dots

#endif // DOTS_HPP
