#pragma once
#include <string>
#include <variant>
#include <vector>
#include "types.h"

struct Expression
{
    explicit Expression(std::string_view string);
    explicit Expression(const Atom &atom, std::vector<Expression> &children)
        : atom(atom), children(std::move(children)) {};

    Atom atom;
    std::vector<Expression> children;
    std::string to_string() const;
    std::string to_human_string() const;
};
