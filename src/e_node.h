#pragma once

#include <string>
#include <vector>
#include <variant>
#include "types.h"

enum class Op
{
    Add,
    Mul,
    Transpose,
    Invert,
    Negate,
    Identity,
    Zero,
    Symbol
};

class ENode
{
private:
    Op op;
    Children children;
    std::variant<std::monostate, int, std::string> payload;

public:
    size_t arity() const;
    Op discriminant() const;
    bool matches(const ENode &other) const;

    // access children (mutable/immutable)
    const Children &get_children() const;
    Children &get_children_mut();

    std::string to_string() const;
    size_t hash() const;
    bool is_leaf() const;
};