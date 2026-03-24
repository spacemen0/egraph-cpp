#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

enum class Op {
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

struct MatrixProperty {
    Shape shape; // (rows, cols)
    struct MatrixFlags {
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
    };
    MatrixFlags flags;

    struct FlagDescriptor {
        bool MatrixFlags::*member;
        const char *label;
    };

    static constexpr std::array<FlagDescriptor, 13> flag_descriptors = {{
        {&MatrixFlags::is_symmetric, "symmetric"},
        {&MatrixFlags::is_orthogonal, "orthogonal"},
        {&MatrixFlags::is_orthonormal, "orthonormal"},
        {&MatrixFlags::is_identity, "identity"},
        {&MatrixFlags::is_zero, "zero"},
        {&MatrixFlags::is_upper_triangular, "upper_triangular"},
        {&MatrixFlags::is_lower_triangular, "lower_triangular"},
        {&MatrixFlags::is_diagonal, "diagonal"},
        {&MatrixFlags::is_positive_definite, "positive_definite"},
        {&MatrixFlags::is_singular, "singular"},
        {&MatrixFlags::is_permutation, "permutation"},
        {&MatrixFlags::is_tall, "tall"},
        {&MatrixFlags::is_wide, "wide"},
    }};

    static MatrixProperty from_string(std::string_view text);

    bool is_square() const { return shape.first == shape.second; }
    bool has_symbolic_shape() const {
        return !std::holds_alternative<int>(shape.first) || !std::holds_alternative<int>(shape.second);
    }
    bool is_tall_matrix() const {
        if (auto rows = std::get_if<int>(&shape.first)) {
            if (auto cols = std::get_if<int>(&shape.second)) {
                return *rows > *cols;
            }
        }
        return flags.is_tall;
    }
    bool is_wide_matrix() const {
        if (auto rows = std::get_if<int>(&shape.first)) {
            if (auto cols = std::get_if<int>(&shape.second)) {
                return *rows < *cols;
            }
        }
        return flags.is_wide;
    }
    bool is_scalar() const {
        if (auto w = std::get_if<int>(&shape.first)) {
            if (auto h = std::get_if<int>(&shape.second)) {
                return *w == 1 && *h == 1;
            }
        }
        return false;
    }
    bool is_vector() const {
        if (auto w = std::get_if<int>(&shape.first)) {
            if (auto h = std::get_if<int>(&shape.second)) {
                return (*w == 1 && *h > 1) || (*h == 1 && *w > 1);
            } else {
                return *w == 1;
            }
        } else if (auto h = std::get_if<int>(&shape.second)) {
            return *h == 1;
        }
        return false;
    }

    bool operator==(const MatrixProperty &other) const { return shape == other.shape; };
    bool strict_equal(const MatrixProperty &other) const {
        if (shape != other.shape) {
            return false;
        }

        for (const auto &flag : flag_descriptors) {
            if (this->flags.*(flag.member) != other.flags.*(flag.member)) {
                return false;
            }
        }

        return true;
    }

    std::string to_string() const {
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

        for (const auto &flag : flag_descriptors) {
            if (this->flags.*(flag.member)) {
                s += " [";
                s += flag.label;
                s += "]";
            }
        }

        return s;
    }
};
using TupleProperty = std::vector<MatrixProperty>;

struct AnalysisData {
    std::variant<MatrixProperty, TupleProperty, std::string> property;
    bool operator==(const AnalysisData &other) const {
        if (property.index() != other.property.index())
            return false;
        if (const auto *p1 = std::get_if<MatrixProperty>(&property)) {
            const auto *p2 = std::get_if<MatrixProperty>(&other.property);
            return *p1 == *p2;
        } else {
            const auto *p3 = std::get_if<TupleProperty>(&property);
            const auto *p2 = std::get_if<TupleProperty>(&other.property);
            return *p3 == *p2;
        }
    }
};

struct ParsedAtom {
    Atom atom;
    std::vector<std::string> children_strings;
};

struct Monomial {
    std::vector<std::string> symbols;
    void normalize() { std::sort(symbols.begin(), symbols.end()); }
    bool operator==(const Monomial &other) const {
        Monomial a = *this;
        Monomial b = other;
        a.normalize();
        b.normalize();
        return a.symbols == b.symbols;
    }
};

namespace std {
template <> struct hash<Monomial> {
    size_t operator()(const Monomial &m) const {
        size_t seed = 0;
        Monomial normalized = m;
        normalized.normalize();
        for (const auto &symbol : normalized.symbols) {
            seed ^= hash<std::string>()(symbol) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

template <> struct equal_to<Monomial> {
    bool operator()(const Monomial &a, const Monomial &b) const { return a == b; }
};
} // namespace std

using SymbolicCost = std::unordered_map<Monomial, double, std::hash<Monomial>, std::equal_to<Monomial>>;
using Cost = std::variant<double, SymbolicCost>;

inline SymbolicCost operator+(const SymbolicCost &a, const SymbolicCost &b) {
    SymbolicCost result = a;
    for (const auto &[monomial, coeff] : b) {
        result[monomial] += coeff;
    }
    return result;
}

inline Cost &operator+=(Cost &lhs, const Cost &rhs) {
    if (lhs.index() != rhs.index()) {
        if (std::holds_alternative<double>(lhs) && std::get<double>(lhs) == 0.0) {
            lhs = rhs;
            return lhs;
        }
        if (std::holds_alternative<double>(rhs) && std::get<double>(rhs) == 0.0) {
            return lhs;
        }
        throw std::invalid_argument("Cost types must match for +=");
    }
    if (std::holds_alternative<double>(lhs)) {
        std::get<double>(lhs) += std::get<double>(rhs);
    } else {
        auto &lhs_map = std::get<SymbolicCost>(lhs);
        const auto &rhs_map = std::get<SymbolicCost>(rhs);
        for (const auto &[k, v] : rhs_map) {
            lhs_map[k] += v;
        }
    }
    return lhs;
}

inline std::ostream &operator<<(std::ostream &os, const Cost &cost) {
    if (std::holds_alternative<double>(cost)) {
        os << std::get<double>(cost);
    } else {
        const auto &symbolic = std::get<SymbolicCost>(cost);
        if (symbolic.empty()) {
            os << "0";
        } else {
            bool first = true;
            for (const auto &[monomial, coeff] : symbolic) {
                if (!first)
                    os << " + ";
                os << coeff;
                for (const auto &symbol : monomial.symbols) {
                    os << "*" << symbol;
                }
                first = false;
            }
        }
    }
    return os;
}
