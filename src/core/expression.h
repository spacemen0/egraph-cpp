#pragma once

#include "basic_types.h"
#include "e_node.h"
#include <string>
#include <vector>

struct Expression {
    explicit Expression() = default;
    explicit Expression(std::string_view string);
    explicit Expression(int i) : atom(i) {}
    explicit Expression(const ScalarExpr &s) : atom(s) {}
    explicit Expression(const Atom &atom, std::vector<Expression> children)
        : atom(atom), children(std::move(children)) {};

    explicit Expression(const ENode &node, const EGraph &egraph);
    Atom atom;
    std::vector<Expression> children;
    std::string to_string(bool readable = false) const;
    bool operator==(const Expression &other) const;
    static std::string render(const Expression &expr, bool readable, int parent_precedence = 0);
};

inline Expression operator+(const Expression &lhs, const Expression &rhs) { return Expression(Op::Add, {lhs, rhs}); }
inline Expression operator-(const Expression &lhs, const Expression &rhs) { return Expression(Op::Minus, {lhs, rhs}); }
inline Expression operator-(const Expression &e) { return Expression(Op::Scale, {e, Expression(-1.0)}); }
inline Expression operator*(const Expression &lhs, const Expression &rhs) { return Expression(Op::Mul, {lhs, rhs}); }
inline Expression transpose(const Expression &e) { return Expression(Op::Tr, {e}); }
inline Expression inverse(const Expression &e) { return Expression(Op::Inv, {e}); }
inline Expression determinant(const Expression &e) { return Expression(Op::Det, {e}); }
inline Expression log(const Expression &e) { return Expression(Op::Log, {e}); }
