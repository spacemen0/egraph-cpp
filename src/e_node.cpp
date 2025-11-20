#include "e_node.h"

#include <functional>
#include <numeric>
#include <string>

size_t ENode::arity() const
{
    return children.size();
}

Op ENode::discriminant() const
{
    return op;
}

bool ENode::matches(const ENode &other) const
{
    if (op != other.op)
        return false;
    if (children.size() != other.children.size())
        return false;
    if (op == Op::Symbol)
        return payload == other.payload;
    return true;
}

const Children &ENode::get_children() const
{
    return children;
}
Children &ENode::get_children_mut()
{
    return children;
}

std::string ENode::to_string() const
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

size_t ENode::hash() const
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

bool ENode::is_leaf() const
{
    return children.empty();
}
