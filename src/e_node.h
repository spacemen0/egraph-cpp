#pragma once

#include <string>
#include <vector>
#include <variant>
#include "types.h"

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
    }
}
class ENode
{
private:
    Children children;
    std::variant<Op, int, std::string> op_or_string;

public:
    explicit ENode(const Children &children, std::variant<Op, int, std::string> const &op_or_string)
        : children(children), op_or_string(op_or_string) {}
    bool matches(const ENode &other) const;

    // access children (mutable/immutable)
    const Children &get_children() const;
    Children &get_children_mut();

    std::string to_string() const;
    size_t hash() const;
    bool is_leaf() const;

    // Declare operator== as a hidden friend
    friend bool operator==(const ENode &a, const ENode &b) noexcept
    {
        return a.matches(b);
    };
    friend bool operator!=(const ENode &a, const ENode &b) noexcept
    {
        return !a.matches(b);
    }
};

struct ENodePtrHash
{
    size_t operator()(const ENode *e) const noexcept
    {
        return e->hash();
    }
};

struct ENodePtrEqual
{
    bool operator()(const ENode *a, const ENode *b) const noexcept
    {
        return *a == *b;
    }
};