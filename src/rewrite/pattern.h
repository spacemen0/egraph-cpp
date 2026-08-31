#pragma once

#include "basic_types.h"
#include <map>
#include <string>
#include <vector>

namespace egraph {
struct Pattern {
    explicit Pattern(const Atom &atom, const std::vector<Pattern> &children) : atom(atom), children(children) {}
    explicit Pattern(std::string_view s);
    Atom atom;
    std::vector<Pattern> children;
};

///@brief
/// a map from a variable's name to the canonical e-class ID
using Substitution = std::map<std::string, Id, std::less<>>;
} // namespace egraph
