#include "expression.h"
#include <stdexcept>
#include "parser_helper.h"

/// @brief parse a expression from a string with the format: Op(child1, child2, ...)
/// @param string
Expression::Expression(std::string_view string)
{
    auto parsed = parse_string_to_atom(string);
    atom = parsed.atom;
    for (const auto &child_str : parsed.children_strings)
    {
        children.emplace_back(child_str); // recursively parse child expressions
    }
}
