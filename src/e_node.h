#pragma once

#include <string>
#include <vector>
#include <variant>
#include "types.h"

class ENode
{
private:
    Children children;
    Atom atom;

public:
    explicit ENode(const Children &children, Atom const &atom)
        : children(children), atom(atom) {}
    bool equals(const ENode &other) const;

    // access children (mutable/immutable)
    const Children &get_children() const;
    Children &get_children_mut();
    Atom get_atom() const;

    std::string to_string() const;
    std::string format() const;
    size_t hash() const;
    bool is_leaf() const;

    // Declare operator== as a hidden friend
    friend bool operator==(const ENode &a, const ENode &b) noexcept
    {
        return a.equals(b);
    };
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