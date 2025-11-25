#pragma once
#include <string>
#include <variant>
#include <vector>
#include "types.h"

struct Expressionvar
{
    std::string name;
};

struct Expression
{
    explicit Expression(std::string_view string);

    std::variant<Op, Expressionvar> op_or_string;
    std::vector<Expression> children;
};
