#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include <string>

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
using Atom = std::variant<Op, std::string>;
