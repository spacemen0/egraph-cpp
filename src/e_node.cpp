#include "e_node.h"

#include <functional>
#include <numeric>
#include <string>

bool ENode::matches(const ENode &other) const
{
    if (op_or_string != other.op_or_string)
        return false;
    if (children.size() != other.children.size())
        return false;
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
    if (std::holds_alternative<Op>(op_or_string))
    {
        Op op = std::get<Op>(op_or_string);
        switch (op)
        {
        case Op::Add:
            return "Add";
        case Op::Mul:
            return "Mul";
        case Op::Transpose:
            return "Transpose";
        case Op::Invert:
            return "Invert";
        case Op::Negate:
            return "Negate";
        case Op::Identity:
            return "Identity";
        case Op::Zero:
            return "Zero";
        }
    }
    else
    {
        return "Str:" + std::get<std::string>(op_or_string);
    }
}

size_t ENode::hash() const
{
    size_t seed;
    if (std::holds_alternative<Op>(op_or_string))
    {
        Op op = std::get<Op>(op_or_string);
        seed = std::hash<int>()(static_cast<int>(op));
    }
    else
    {
        // use a fixed discriminant for string payloads;
        seed = std::hash<int>()(-1);
    }

    seed = std::accumulate(children.begin(), children.end(), seed,
                           [](size_t acc, Id c)
                           {
                               size_t hc = std::hash<Id>()(c);
                               return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
                           });

    if (!std::holds_alternative<Op>(op_or_string))
    {
        const auto &s = std::get<std::string>(op_or_string);
        size_t hp = std::hash<std::string>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const
{
    return children.empty();
}
