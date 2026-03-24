#pragma once

#include "basic_types.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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
