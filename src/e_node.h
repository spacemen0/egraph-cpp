#pragma once

#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <ostream>
#include "types.h"
#include <numeric>

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

struct ENode
{
    Op op;
    Children children;
    std::variant<std::monostate, int, std::string> payload;
    size_t arity() const { return children.size(); }

    Op discriminant() const { return op; }

    bool matches(const ENode &other) const
    {
        if (op != other.op)
            return false;
        if (children.size() != other.children.size())
            return false;
        if (op == Op::Symbol)
            return payload == other.payload;
        return true;
    }

    // access children (mutable/immutable)
    const Children &get_children() const { return children; }
    Children &get_children_mut() { return children; }

    std::string to_string() const
    {
        switch (op)
        {
        case Op::Add:
            return "(+)";
        case Op::Mul:
            return "(*)";
        case Op::Transpose:
            return "transpose";
        case Op::Invert:
            return "invert";
        case Op::Negate:
            return "negate";
        case Op::Identity:
            return "Identity";
        case Op::Zero:
            return "Zero";
        case Op::Symbol:
            return std::get<std::string>(payload);
        }
        return "?";
    }

    size_t hash() const
    {
        // start with op discriminant
        size_t seed = std::hash<int>()(static_cast<int>(op));

        seed = std::accumulate(children.begin(), children.end(), seed,
                               [](size_t acc, Id c)
                               {
                                   size_t hc = std::hash<Id>()(c);
                                   return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
                               });

        if (op == Op::Symbol)
        {
            const auto &s = std::get<std::string>(payload);
            size_t hp = std::hash<std::string>()(s);
            seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        return seed;
    }
};