#pragma once

#include "basic_types.h"
#include "types.h"
#include <string_view>

namespace egraph {
namespace parser {

// Parses an expression string (infix or prefix) into a ParsedAtom.
// The children_strings in the returned ParsedAtom will be formatted
// consistently so they can be recursively parsed.
ParsedAtom parse_expression(std::string_view s);

// Parses a scalar expression string (e.g. "k - 1.0", "(a + b) / 2") into a ScalarExpr.
ScalarExpr parse_scalar(std::string_view s);

} // namespace parser

} // namespace egraph
