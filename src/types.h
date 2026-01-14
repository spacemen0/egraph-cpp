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
using Size = std::variant<int, std::string>;

struct MatrixProperty
{
    std::pair<Size, Size> shape;
    bool is_symmetric = false;
    bool is_orthogonal = false;

    bool is_square() const { return shape.first == shape.second; }
};

struct AnalysisData
{
    MatrixProperty property;
};

struct ParsedAtom
{
    Atom atom;
    std::vector<std::string> children_strings;
};
