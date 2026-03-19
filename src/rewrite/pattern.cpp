#include "pattern.h"
#include "utils.h"
#include <algorithm>
#include <iterator>

/// @brief Construct a Pattern from a string representation
/// @param s
Pattern::Pattern(std::string_view s) {
    auto parsed = string_to_parsed_atom(s);
    atom = parsed.atom;
    std::ranges::transform(parsed.children_strings, std::back_inserter(children), [](const std::string &str) {
        return Pattern(str);
    });
}