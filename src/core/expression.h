#pragma once

#include "basic_types.h"
#include "e_node.h"
#include <string>
#include <vector>

struct Expression {
    explicit Expression(std::string_view string);
    explicit Expression(const Atom &atom, std::vector<Expression> &children)
        : atom(atom), children(std::move(children)) {};
    explicit Expression(const ENode &node, const EGraph &egraph);
    Atom atom;
    std::vector<Expression> children;
    std::string to_string() const;
    bool operator==(const Expression &other) const;
};
