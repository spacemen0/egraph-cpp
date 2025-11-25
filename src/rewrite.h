#pragma once
#include "expression.h"

struct Rewrite
{
    std::string name;
    Expression lhs;
    Expression rhs;
};