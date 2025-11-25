#pragma once
#include <cstdint>
#include <vector>

enum class Op
{
    Add,
    Mul,
    Transpose,
    Invert,
    Negate,
    Identity,
    Zero,
};

using Id = uint32_t;
using Children = std::vector<Id>;
using Term = std::variant<Op, std::string>;
