#pragma once

#include <string>
#include <variant>
#include <vector>
#include <map>
#include "types.h"

/// @brief Represents a variable in a pattern, "?x".
struct PatternVar
{
    std::string name;

    bool operator==(const PatternVar &other) const = default;
};

using PatternAtom = std::variant<Op, PatternVar>;

struct Pattern
{
    explicit Pattern(const PatternAtom &atom, const std::vector<Pattern> &children)
        : atom(atom), children(children) {}
    explicit Pattern(std::string_view s);
    PatternAtom atom;
    std::vector<Pattern> children;
};

///@brief
/// a map from a variable's name to the canonical e-class ID
using Substitution = std::map<std::string, Id>;