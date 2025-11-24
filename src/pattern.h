#pragma once
#include <string>
#include <variant>
#include <vector>
#include "types.h"

struct PatternVar
{
    std::string name;
};

struct Pattern
{
    explicit Pattern(std::string_view string);

    std::variant<Op, PatternVar> op_or_var;
    std::vector<Pattern> children;
};
