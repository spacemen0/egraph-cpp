#pragma once

#include <string>
#include <vector>
#include <variant>
#include "types.h"

class ENode
{
private:
    Children children;
    Term op_or_string;

public:
    explicit ENode(const Children &children, Term const &op_or_string)
        : children(children), op_or_string(op_or_string) {}
    bool equals(const ENode &other) const;

    // access children (mutable/immutable)
    const Children &get_children() const;
    Children &get_children_mut();

    std::string to_string() const;
    size_t hash() const;
    bool is_leaf() const;

    // Declare operator== as a hidden friend
    friend bool operator==(const ENode &a, const ENode &b) noexcept
    {
        return a.equals(b);
    };
    friend bool operator!=(const ENode &a, const ENode &b) noexcept
    {
        return !a.equals(b);
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