#include "pattern.h"
#include "parser_helper.h"

/// @brief Construct a Pattern from a string representation
/// @param s
Pattern::Pattern(std::string_view s)
{
    auto parsed = parse_string_to_atom(s);
    atom = parsed.atom;
    for (const auto &child_str : parsed.children_strings)
    {
        children.emplace_back(child_str); // recursively parse child patterns
    }
}