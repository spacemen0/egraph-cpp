#pragma once

#include "basic_types.h"
#include "e_node.h"
#include "op_registry.h"
#include <string>
#include <utility>
#include <vector>

namespace egraph {
struct Expression {

    Atom atom;
    std::vector<Expression> children;

    explicit Expression() = default;
    explicit Expression(std::string_view string);
    explicit Expression(int i) : atom(i) {}
    explicit Expression(double v) : atom(ScalarExpr(v)) {}
    explicit Expression(const ScalarExpr &s) : atom(s) {}
    explicit Expression(Atom atom, std::vector<Expression> children)
        : atom(std::move(atom)), children(std::move(children)) {};

    explicit Expression(const ENode &node, const EGraph &egraph);

    std::string to_string(bool readable = false) const;
    bool operator==(const Expression &other) const;
    size_t depth() const;
    size_t node_count() const;
    static std::string render(const Expression &expr, bool readable, int parent_precedence = 0);
};

inline Expression operator+(const Expression &lhs, const Expression &rhs) {
    return Expression(OpRegistry::op("Add"), {lhs, rhs});
}
inline Expression operator-(const Expression &lhs, const Expression &rhs) {
    return Expression(OpRegistry::op("Minus"), {lhs, rhs});
}
inline Expression operator-(const Expression &e) { return Expression(OpRegistry::op("Scale"), {e, Expression(-1.0)}); }
inline Expression operator*(const Expression &lhs, const Expression &rhs) {
    return Expression(OpRegistry::op("Mul"), {lhs, rhs});
}
inline Expression transpose(const Expression &e) { return Expression(OpRegistry::op("Tr"), {e}); }
inline Expression inverse(const Expression &e) { return Expression(OpRegistry::op("Inv"), {e}); }
inline Expression scale(const Expression &e, const ScalarExpr &s) {
    return Expression(OpRegistry::op("Scale"), {e, Expression(s)});
}
inline Expression scale(const Expression &e, double v) {
    return Expression(OpRegistry::op("Scale"), {e, Expression(ScalarExpr(v))});
}

inline bool is_subexpression(const Expression &parent, const Expression &target) {
    if (parent == target) {
        return true;
    }
    for (const auto &child : parent.children) {
        if (is_subexpression(child, target)) {
            return true;
        }
    }
    return false;
}

} // namespace egraph
