#pragma once

#include "basic_types.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>


namespace egraph {
struct ParsedAtom {
    Atom atom;
    std::vector<std::string> children_strings;
};

struct Monomial {
    std::vector<std::string> symbols;
    Monomial(std::vector<std::string> s) : symbols(std::move(s)) { normalize(); }
    void normalize() { std::sort(symbols.begin(), symbols.end()); }
    bool operator==(const egraph::Monomial &other) const { return symbols == other.symbols; }
};

} // namespace egraph
namespace std {
template <> struct hash<egraph::Monomial> {
    size_t operator()(const egraph::Monomial &m) const {
        size_t seed = 0;
        for (const auto &symbol : m.symbols) {
            seed ^= ::std::hash<std::string>()(symbol) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
template <> struct equal_to<egraph::Monomial> {
    bool operator()(const egraph::Monomial &a, const egraph::Monomial &b) const { return a == b; }
};
} // namespace std
namespace egraph {



using SymbolicCost = std::unordered_map<Monomial, double, std::hash<egraph::Monomial>, std::equal_to<egraph::Monomial>>;
using Cost = std::variant<double, SymbolicCost>;

inline SymbolicCost operator+(const SymbolicCost &a, const SymbolicCost &b) {
    SymbolicCost result = a;
    for (const auto &[monomial, coeff] : b) {
        result[monomial] += coeff;
    }
    return result;
}

inline Cost operator*(const Cost &lhs, double rhs) {
    if (std::holds_alternative<double>(lhs)) {
        return std::get<double>(lhs) * rhs;
    } else {
        SymbolicCost result;
        for (const auto &[monomial, coeff] : std::get<SymbolicCost>(lhs)) {
            result[monomial] = coeff * rhs;
        }
        return result;
    }
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

inline bool strictly_dominates(const SymbolicCost &a, const SymbolicCost &b) {
    int deg_a = -1, deg_b = -1;
    for (const auto &[m, c] : a)
        if (c > 0)
            deg_a = std::max(deg_a, (int)m.symbols.size());
    for (const auto &[m, c] : b)
        if (c > 0)
            deg_b = std::max(deg_b, (int)m.symbols.size());

    if (deg_a > deg_b)
        return true; // a is asymptotically worse
    if (deg_a < deg_b)
        return false;

    // If same degree, fall back to strict coefficient dominance
    bool strictly_greater = false;
    for (const auto &[monomial, coeff_b] : b) {
        auto it = a.find(monomial);
        if (it == a.end() || it->second < coeff_b) {
            return false;
        }
        if (it->second > coeff_b) {
            strictly_greater = true;
        }
    }
    for (const auto &[monomial, coeff_a] : a) {
        if (b.find(monomial) == b.end() && coeff_a > 0) {
            strictly_greater = true;
        }
    }
    return strictly_greater;
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

struct PruneResult {
    size_t nodes_before = 0;
    size_t nodes_after = 0;
    size_t nodes_pruned = 0;
    size_t classes_with_removed_nodes = 0;
    bool changed = false;
};
} // namespace egraph

