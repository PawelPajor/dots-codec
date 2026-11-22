// SPDX-License-Identifier: MIT OR Apache-2.0
// Copyright (c) 2026 Pawel Pajor

#include "dots.hpp"

static_assert(dots::code_name(dots::code::ok) == "OK");
static_assert(dots::code_name(dots::code::invalid_text) == "INVALID_TEXT");
static_assert(dots::code_name(static_cast<dots::code>(9)).empty());

auto main() -> int { return 0; }
