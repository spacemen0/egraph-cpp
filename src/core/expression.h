#pragma once

#include "basic_types.h"
#include "e_node.h"
#include <string>
#include <vector>

struct EGraph;
struct ENode;

struct Expression {
    explicit Expression(std::string_view string);
    explicit Expression(const Atom &atom, std::vector<Expression> &children)
        : atom(atom), children(std::move(children)) {};

    struct FromAtomTag {};
    explicit Expression(const Atom &atom, FromAtomTag) : atom(atom), children({}) {};

    Expression(const Expression &) = default;
    Expression(Expression &&) = default;
    Expression &operator=(const Expression &) = default;
    Expression &operator=(Expression &&) = default;

    explicit Expression(const ENode &node, const EGraph &egraph);
    Atom atom;
    std::vector<Expression> children;
    std::string to_string() const;
    std::string to_human_string() const;
    bool operator==(const Expression &other) const;
};
