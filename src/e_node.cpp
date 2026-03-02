#include "e_node.h"
#include "e_graph.h"
#include <stdexcept>
#include <functional>
#include <numeric>
#include <string>
#include <format>
#include <unordered_set>
#include <algorithm>

double ENode::compute_cost(const EGraph &egraph) const
{
    return 1.0;
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
        case Tr:
            return "Tr";
        case Inv:
            return "Inv";
        case Neg:
            return "Neg";
        case QR:
            return "QR";
        case LU:
            return "LU";
        case LLt:
            return "LLt";
        case Get:
            return "Get";
        case Sol:
            return "Sol";
        case TriSol:
            return "TriSol";
        case Det:
            return "Det";
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
    std::ranges::for_each(children, [&](Id child_id)
                          { str += " " + std::to_string(child_id); });
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

static bool has_ancestor_impl(
    const ENode &node,
    std::string_view ancestor_op,
    const EGraph &egraph,
    std::unordered_set<Id> &visited)
{
    if (std::holds_alternative<Op>(node.get_atom()) && node.to_string() == ancestor_op)
    {
        return true;
    }
    auto opt_id = egraph.find_node_id(node);
    if (!opt_id.has_value())
        return false;

    Id this_class_id = egraph.find_class_id(opt_id.value());
    if (!visited.insert(this_class_id).second)
    {
        return false;
    }

    auto parent_ids = egraph.get_class_parents(this_class_id);
    for (Id parent_id : parent_ids)
    {
        const ENode &parent_node = egraph.at(parent_id);
        if (has_ancestor_impl(parent_node, ancestor_op, egraph, visited))
        {
            return true;
        }
    }
    return false;
}

bool ENode::has_ancestor(std::string_view ancestor_op, const EGraph &egraph) const
{
    std::unordered_set<Id> visited;
    return has_ancestor_impl(*this, ancestor_op, egraph, visited);
}