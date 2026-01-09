#pragma once

#include <string>
#include <variant>
#include <vector>
#include <map>
#include "types.h"

struct Pattern
{
    explicit Pattern(const Atom &atom, const std::vector<Pattern> &children)
        : atom(atom), children(children) {}
    explicit Pattern(std::string_view s);
    Atom atom;
    std::vector<Pattern> children;
};

///@brief
/// a map from a variable's name to the canonical e-class ID
using Substitution = std::map<std::string, Id>;