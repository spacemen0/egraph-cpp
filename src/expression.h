#pragma once
#include <string>
#include <variant>
#include <vector>
#include "types.h"

struct Expression
{
    explicit Expression(std::string_view string);

    Atom atom;
    std::vector<Expression> children;
};
