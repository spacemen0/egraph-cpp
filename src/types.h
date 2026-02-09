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
    QR,              // output: [Q, R] assume full QR
    LU,              // output: [L, U, P]
    LLt,             // output: [L, P]
    Get,             // [tuple, index]
    Solve,           // [A, B] solving AX = B
    TriangularSolve, // [A, B] solving AX = B where A is triangular
    Determinant,     // [A] computing det(A)
    Log,             //
};

using Id = uint32_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string, int>; // int for indexes in Get operations
using Size = std::variant<int, std::string>;

struct MatrixProperty
{
    std::pair<Size, Size> shape; // (rows, cols)
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
    bool is_tall = false;
    bool is_wide = false;

    bool is_square() const { return shape.first == shape.second; }
    bool has_symbolic_shape() const
    {
        return !std::holds_alternative<int>(shape.first) || !std::holds_alternative<int>(shape.second);
    }
    bool is_tall_matrix() const
    {
        if (auto rows = std::get_if<int>(&shape.first))
        {
            if (auto cols = std::get_if<int>(&shape.second))
            {
                return *rows > *cols;
            }
        }
        return is_tall;
    }
    bool is_wide_matrix() const
    {
        if (auto rows = std::get_if<int>(&shape.first))
        {
            if (auto cols = std::get_if<int>(&shape.second))
            {
                return *rows < *cols;
            }
        }
        return is_wide;
    }
    bool is_scalar() const
    {
        if (auto w = std::get_if<int>(&shape.first))
        {
            if (auto h = std::get_if<int>(&shape.second))
            {
                return *w == 1 && *h == 1;
            }
        }
        return false;
    }
    bool is_vector() const
    {
        if (auto w = std::get_if<int>(&shape.first))
        {
            if (auto h = std::get_if<int>(&shape.second))
            {
                return (*w == 1 && *h > 1) || (*h == 1 && *w > 1);
            }
            else
            {
                return *w == 1;
            }
        }
        else if (auto h = std::get_if<int>(&shape.second))
        {
            return *h == 1;
        }
        return false;
    }

    bool operator==(const MatrixProperty &other) const
    {
        return shape == other.shape;
    };
    bool strict_equal(const MatrixProperty &other) const
    {
        return shape == other.shape &&
               is_symmetric == other.is_symmetric &&
               is_orthogonal == other.is_orthogonal &&
               is_identity == other.is_identity &&
               is_zero == other.is_zero &&
               is_upper_triangular == other.is_upper_triangular &&
               is_lower_triangular == other.is_lower_triangular &&
               is_diagonal == other.is_diagonal &&
               is_positive_definite == other.is_positive_definite &&
               is_singular == other.is_singular &&
               is_permutation == other.is_permutation;
    }

    std::string to_string() const
    {
        std::string s = "Matrix(";
        if (auto w = std::get_if<int>(&shape.first))
            s += std::to_string(*w);
        else
            s += std::get<std::string>(shape.first);

        s += "x";

        if (auto h = std::get_if<int>(&shape.second))
            s += std::to_string(*h);
        else
            s += std::get<std::string>(shape.second);

        s += ")";

        if (is_identity)
            s += " [Identity]";
        if (is_zero)
            s += " [Zero]";
        if (is_symmetric)
            s += " [Symmetric]";
        if (is_orthogonal)
            s += " [Orthogonal]";
        if (is_upper_triangular)
            s += " [UpperTriangular]";
        if (is_lower_triangular)
            s += " [LowerTriangular]";
        if (is_diagonal)
            s += " [Diagonal]";
        if (is_positive_definite)
            s += " [PositiveDefinite]";
        if (is_singular)
            s += " [Singular]";
        if (is_permutation)
            s += " [Permutation]";

        return s;
    }
};
using TupleProperty = std::vector<MatrixProperty>;

struct AnalysisData
{
    std::variant<MatrixProperty, TupleProperty> property;
    bool operator==(const AnalysisData &other) const
    {
        if (property.index() != other.property.index())
            return false;
        if (const auto *p1 = std::get_if<MatrixProperty>(&property))
        {
            const auto *p2 = std::get_if<MatrixProperty>(&other.property);
            return *p1 == *p2;
        }
        else
        {
            const auto *p3 = std::get_if<TupleProperty>(&property);
            const auto *p2 = std::get_if<TupleProperty>(&other.property);
            return *p3 == *p2;
        }
    }
};

struct ParsedAtom
{
    Atom atom;
    std::vector<std::string> children_strings;
};
