#include "e_node.h"
#include <stdexcept>
#include <functional>
#include <numeric>
#include <string>
#include <format>

bool ENode::equals(const ENode &other) const
{
    if (atom != other.atom)
        return false;
    if (children.size() != other.children.size())
        return false;
    for (size_t i = 0; i < children.size(); ++i)
    {
        if (children[i] != other.children[i])
            return false;
    }
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

Atom ENode::get_atom() const
{
    return atom;
}

std::string ENode::to_string() const
{
    if (std::holds_alternative<Op>(atom))
    {
        Op op = std::get<Op>(atom);
        switch (op)
        {
            using enum Op;
        case Add:
            return "Add";
        case Mul:
            return "Mul";
        case Transpose:
            return "Transpose";
        case Invert:
            return "Invert";
        case Negate:
            return "Negate";
        default:
            throw std::invalid_argument("Unknown Op in ENode::to_string");
        }
    }
    else
    {
        return std::get<std::string>(atom);
    }
}

std::string ENode::format() const
{
    if (is_leaf())
    {
        return to_string();
    }

    std::string str = "(" + to_string();
    for (Id child : children)
    {
        str += std::format(" {}", child);
    }
    str += ")";
    return str;
}

size_t ENode::hash() const
{
    size_t seed;
    if (std::holds_alternative<Op>(atom))
    {
        Op op = std::get<Op>(atom);
        seed = std::hash<int>()(static_cast<std::underlying_type_t<Op>>(op));
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

    // if string payloads, mix in the string hash
    if (!std::holds_alternative<Op>(atom))
    {
        const auto &s = std::get<std::string>(atom);
        size_t hp = std::hash<std::string>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const
{
    return children.empty();
}
