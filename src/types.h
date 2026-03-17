#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <iostream>

enum class Op
{
    Add,
    Mul,
    Tr,
    Inv,
    Neg,
    QR,     // output: [Q, R]
    LU,     // output: [L, U, P]
    LLt,    // output: [L] where A = LLt
    Get,    // [tuple, index]
    Sol,    // [A, B] solving AX = B
    TriSol, // [A, B] solving AX = B where A is triangular
    Det,    // [A] computing det(A)
    Log,    //
};

using Id = uint32_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string, int>; // int for indexes in Get operations
using Size = std::variant<int, std::string>;
using Shape = std::pair<Size, Size>;
using SizeBindings = std::unordered_map<std::string, int>;

struct MatrixProperty
{
    Shape shape; // (rows, cols)
    bool is_symmetric = false;
    bool is_orthogonal = false;
    bool is_orthonormal = false;
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
               is_orthonormal == other.is_orthonormal &&
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
        if (is_orthonormal)
            s += " [Orthonormal]";
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
    std::variant<MatrixProperty, TupleProperty, std::string> property;
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

struct Monomial
{
    std::vector<std::string> symbols;
    void normalize() { std::sort(symbols.begin(), symbols.end()); }
    bool operator==(const Monomial &other) const
    {
        Monomial a = *this;
        Monomial b = other;
        a.normalize();
        b.normalize();
        return a.symbols == b.symbols;
    }
};

namespace std
{
    template <>
    struct hash<Monomial>
    {
        size_t operator()(const Monomial &m) const
        {
            size_t seed = 0;
            Monomial normalized = m;
            normalized.normalize();
            for (const auto &symbol : normalized.symbols)
            {
                seed ^= hash<std::string>()(symbol) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    template <>
    struct equal_to<Monomial>
    {
        bool operator()(const Monomial &a, const Monomial &b) const
        {
            return a == b;
        }
    };
}

using SymbolicCost = std::unordered_map<Monomial, double, std::hash<Monomial>, std::equal_to<Monomial>>;
using Cost = std::variant<double, SymbolicCost>;

inline SymbolicCost operator+(const SymbolicCost &a, const SymbolicCost &b)
{
    SymbolicCost result = a;
    for (const auto &[monomial, coeff] : b)
    {
        result[monomial] += coeff;
    }
    return result;
}

inline Cost &operator+=(Cost &lhs, const Cost &rhs)
{
    if (lhs.index() != rhs.index())
    {
        if (std::holds_alternative<double>(lhs) && std::get<double>(lhs) == 0.0)
        {
            lhs = rhs;
            return lhs;
        }
        if (std::holds_alternative<double>(rhs) && std::get<double>(rhs) == 0.0)
        {
            return lhs;
        }
        throw std::invalid_argument("Cost types must match for +=");
    }
    if (std::holds_alternative<double>(lhs))
    {
        std::get<double>(lhs) += std::get<double>(rhs);
    }
    else
    {
        auto &lhs_map = std::get<SymbolicCost>(lhs);
        const auto &rhs_map = std::get<SymbolicCost>(rhs);
        for (const auto &[k, v] : rhs_map)
        {
            lhs_map[k] += v;
        }
    }
    return lhs;
}

inline std::ostream &operator<<(std::ostream &os, const Cost &cost)
{
    if (std::holds_alternative<double>(cost))
    {
        os << std::get<double>(cost);
    }
    else
    {
        const auto &symbolic = std::get<SymbolicCost>(cost);
        bool first = true;
        for (const auto &[monomial, coeff] : symbolic)
        {
            if (!first)
                os << " + ";
            os << coeff;
            for (const auto &symbol : monomial.symbols)
            {
                os << "*" << symbol;
            }
            first = false;
        }
    }
    return os;
}
