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
    QR,  // output: [Q, R]
    LU,  // output: [L, U, P]
    LLt, // output: [L, P]
    Get, // [tuple, index]

};

using Id = uint32_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string, int>; // int for indexes in Get operations
using Size = std::variant<int, std::string>;

struct MatrixProperty
{
    std::pair<Size, Size> shape;
    bool is_symmetric = false;
    bool is_orthogonal = false;
    bool is_identity = false;
    bool is_zero = false;
    bool is_upper_triangular = false;
    bool is_lower_triangular = false;
    bool is_diagonal = false;
    bool is_positive_definite = false;
    bool is_singular = false;
    bool is_permutation = false;

    bool is_square() const { return shape.first == shape.second; }

    friend bool operator==(const MatrixProperty &, const MatrixProperty &) = default;
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
