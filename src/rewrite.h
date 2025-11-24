#pragma once
#include "pattern.h"

struct Rewrite
{
    std::string name;
    Pattern lhs;
    Pattern rhs;
};