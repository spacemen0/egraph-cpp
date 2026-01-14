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
    bool is_identity = false;
    bool is_zero = false;

    bool is_square() const { return shape.first == shape.second; }

    bool operator==(const MatrixProperty &other) const
    {
        return shape == other.shape &&
               is_symmetric == other.is_symmetric &&
               is_orthogonal == other.is_orthogonal &&
               is_identity == other.is_identity &&
               is_zero == other.is_zero;
    }
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
