#pragma once
#include <string>
#include <variant>
#include <vector>
#include "types.h"

struct Expression
{
    explicit Expression(std::string_view string);

    Term op_or_string;
    std::vector<Expression> children;
};
