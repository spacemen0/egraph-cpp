#pragma once
#include <cstdint>
#include <vector>

using Id = uint32_t;
using Children = std::vector<Id>;
enum class Op
{
    Add,
    Mul,
    Transpose,
    Invert,
    Negate,
    Identity,
    Zero,
    Symbol
};