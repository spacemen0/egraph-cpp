#include "pattern.h"
#include "utils.h"
#include <algorithm>
#include <iterator>

/// @brief Construct a Pattern from a string representation (usually for rewrite rules)
/// @param s

namespace egraph {
Pattern::Pattern(std::string_view s) {
    auto parsed = string_to_parsed_atom(s);
    atom = parsed.atom;

    if (std::holds_alternative<Op>(atom) && std::get<Op>(atom) == Op::Scale && parsed.children_strings.size() == 2) {
        children.push_back(Pattern(parsed.children_strings[0]));
        const std::string &scalar_str = parsed.children_strings[1];
        if (scalar_str.starts_with('?')) {
            children.push_back(Pattern(scalar_str));
        } else {
            children.push_back(Pattern(Atom(parser::parse_scalar(scalar_str)), {}));
        }
        return;
    }
    std::ranges::transform(parsed.children_strings, std::back_inserter(children), [](const std::string &str) {
        return Pattern(str);
    });
}
} // namespace egraph
