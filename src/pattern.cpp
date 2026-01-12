#include "pattern.h"
#include "utils.h"

/// @brief Construct a Pattern from a string representation
/// @param s
Pattern::Pattern(std::string_view s)
{
    auto parsed = string_to_parsed_atom(s);
    atom = parsed.atom;
    for (const auto &child_str : parsed.children_strings)
    {
        children.emplace_back(child_str); // recursively parse child patterns
    }
}