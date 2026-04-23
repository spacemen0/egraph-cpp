#pragma once

#include "types.h"
#include <string_view>

namespace parser {

// Parses an expression string (infix or prefix) into a ParsedAtom.
// The children_strings in the returned ParsedAtom will be formatted
// consistently so they can be recursively parsed.
ParsedAtom parse_expression(std::string_view s);

} // namespace parser
