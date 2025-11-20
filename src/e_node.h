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

constexpr std::size_t op_arity(Op op) noexcept
{
    switch (op)
    {
    case Op::Add:
        return 2;
    case Op::Mul:
        return 2;
    case Op::Transpose:
        return 1;
    case Op::Invert:
        return 1;
    case Op::Negate:
        return 1;
    case Op::Identity:
        return 0;
    case Op::Zero:
        return 0;
    case Op::Symbol:
        return 0; // symbol is a leaf
    }
    return 0;
}

class ENode
{
private:
    Op op;
    Children children;
    std::variant<std::monostate, int, std::string> payload;

public:
    ENode(Op op, const Children &children = {}, std::variant<std::monostate, int, std::string> payload = {})
        : op(op), children(children), payload(payload) {}
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