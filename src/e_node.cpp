#include "e_node.h"
#include "e_graph.h"
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
        case QR:
            return "QR";
        case LU:
            return "LU";
        case LLt:
            return "LLt";
        case Get:
            return "Get";
        case Solve:
            return "Solve";
        case TriangularSolve:
            return "TriangularSolve";
        case Determinant:
            return "Determinant";
        case Log:
            return "Log";
        default:
            throw std::invalid_argument("Unknown Op in ENode::to_string");
        }
    }
    else if (std::holds_alternative<std::string>(atom))
    {
        return std::get<std::string>(atom);
    }
    else if (std::holds_alternative<int>(atom))
    {
        return std::to_string(std::get<int>(atom));
    }
    throw std::invalid_argument("Unknown atom type in ENode::to_string");
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
    else if (std::holds_alternative<std::string>(atom))
    {
        // use a fixed discriminant for string payloads;
        seed = std::hash<int>()(-1);
    }
    else
    {
        // int payload
        seed = std::hash<int>()(-2);
    }

    seed = std::accumulate(children.begin(), children.end(), seed,
                           [](size_t acc, Id c)
                           {
                               size_t hc = std::hash<Id>()(c);
                               return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
                           });

    // if string payloads, mix in the string hash
    if (std::holds_alternative<std::string>(atom))
    {
        const auto &s = std::get<std::string>(atom);
        size_t hp = std::hash<std::string>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
    else if (std::holds_alternative<int>(atom))
    {
        int i = std::get<int>(atom);
        size_t hp = std::hash<int>()(i);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const
{
    return children.empty();
}

bool ENode::has_ancestor(std::string_view ancestor_op, const EGraph &egraph) const
{
    if (std::holds_alternative<Op>(atom) && to_string() == ancestor_op)
    {
        return true;
    }
    auto opt_id = egraph.find_node_id(*this);
    if (!opt_id.has_value())
        return false;

    Id this_id = opt_id.value();
    auto parent_ids = egraph.get_class_parents(egraph.find_class_id(this_id));
    {
        for (Id parent_id : parent_ids)
        {
            if (egraph.find_class_id(parent_id) != parent_id) // to prevent stack overflow
                continue;
            const ENode &parent_node = egraph.at(parent_id);
            if (parent_node.has_ancestor(ancestor_op, egraph))
            {
                return true;
            }
        }
        return false;
    }
}