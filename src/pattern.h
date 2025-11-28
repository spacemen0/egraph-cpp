#pragma once

#include <string>
#include <variant>
#include <vector>
#include <map>
#include "types.h"

/// @brief Represents a variable in a pattern, e.g., "?x".
struct PatternVar
{
    std::string name;

    bool operator==(const PatternVar &other) const
    {
        return name == other.name;
    }
};

/// @brief The atom of a pattern. It can be a fixed operator (Op),
/// or a variable(PatternVar).
using PatternAtom = std::variant<Op, PatternVar>;

struct Pattern
{
    PatternAtom atom;
    std::vector<Pattern> children;
};

///@brief Represents a single match
/// a map from a variable's name (e.g., "?x") to the canonical e-class ID
using Substitution = std::map<std::string, Id>;