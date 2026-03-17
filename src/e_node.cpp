#include "e_node.h"
#include "e_graph.h"
#include "types.h"
#include "utils.h"
#include <stdexcept>
#include <functional>
#include <numeric>
#include <string>
#include <format>
#include <unordered_set>
#include <algorithm>

namespace
{
    Size bind_size(const Size &size, const SizeBindings *size_bindings)
    {
        if (!size_bindings)
        {
            return size;
        }

        if (const auto *symbol = std::get_if<std::string>(&size))
        {
            if (auto it = size_bindings->find(*symbol); it != size_bindings->end())
            {
                return it->second;
            }
        }

        return size;
    }

    Shape bind_shape(const Shape &shape, const SizeBindings *size_bindings)
    {
        return {bind_size(shape.first, size_bindings), bind_size(shape.second, size_bindings)};
    }

    std::string size_to_symbol(const Size &size)
    {
        if (const auto *value = std::get_if<int>(&size))
        {
            return std::to_string(*value);
        }
        return std::get<std::string>(size);
    }
}

Cost ENode::compute_local_cost(const EGraph &egraph, const SizeBindings *size_bindings) const
{
    auto get_one_shape = [&](Id child_id) -> std::pair<std::variant<int, std::string>, std::variant<int, std::string>>
    {
        auto data = get_matrix_data(egraph, child_id);
        if (data)
        {
            return bind_shape(data->shape, size_bindings);
        }
        return {{}, {}};
    };
    auto get_two_shapes = [&](Id child_id1, Id child_id2) -> std::pair<std::pair<std::variant<int, std::string>, std::variant<int, std::string>>,
                                                                       std::pair<std::variant<int, std::string>, std::variant<int, std::string>>>
    {
        auto shape1 = get_one_shape(child_id1);
        auto shape2 = get_one_shape(child_id2);
        return {shape1, shape2};
    };
    if (is_leaf())
    {
        return 0.0;
    }
    if (auto op = std::get_if<Op>(&atom))
    {
        switch (*op)
        {
            using enum Op;
        case Add:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                return static_cast<double>(rows * cols);
            }
            if (is_symbolic(shape))
            {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Add operation in ENode::compute_local_cost");
        }
        case Mul:
        {
            auto shapes = get_two_shapes(children.at(0), children.at(1));
            if (is_concrete(shapes.first) && is_concrete(shapes.second))
            {
                int rows1 = std::get<int>(shapes.first.first);
                int cols1 = std::get<int>(shapes.first.second);
                int cols2 = std::get<int>(shapes.second.second);
                return static_cast<double>(rows1 * cols1 * cols2);
            }
            if (is_symbolic(shapes.first) && is_symbolic(shapes.second))
            {
                Monomial m = {{size_to_symbol(shapes.first.first), size_to_symbol(shapes.first.second), size_to_symbol(shapes.second.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shapes for Mul operation in ENode::compute_local_cost");
        }
        case Tr:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                return static_cast<double>(rows * cols);
            }
            if (is_symbolic(shape))
            {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Tr operation in ENode::compute_local_cost");
        }
        case Inv:
        {
            auto shape = get_one_shape(children.at(0));
            auto data = get_matrix_data(egraph, egraph.find_node_id(*this).value());
            if (is_concrete(shape))
            {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                if (rows != cols)
                {
                    throw std::invalid_argument("Non-square matrix for Inv operation in ENode::compute_local_cost");
                }
                if (data && (data->is_upper_triangular || data->is_lower_triangular))
                {
                    return (1.0 / 3.0) * rows * rows * rows;
                }
                return static_cast<double>(rows * rows * rows);
            }
            if (is_symbolic(shape))
            {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.first), size_to_symbol(shape.first)}};
                SymbolicCost sc;
                if (data && (data->is_upper_triangular || data->is_lower_triangular))
                {
                    sc[m] = 1.0 / 3.0;
                }
                else
                {
                    sc[m] = 1.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Inv operation in ENode::compute_local_cost");
        };
        case Neg:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                return static_cast<double>(rows * cols);
            }
            if (is_symbolic(shape))
            {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Neg operation in ENode::compute_local_cost");
        };
        case QR:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                return (2.0 * rows * cols * cols) - ((2.0 / 3.0) * cols * cols * cols);
            }
            if (is_symbolic(shape))
            {
                std::string r = size_to_symbol(shape.first);
                std::string c = size_to_symbol(shape.second);

                Monomial mn2 = {{r, c, c}};
                Monomial n3 = {{c, c, c}};

                SymbolicCost sc;
                sc[mn2] = 2.0;
                sc[n3] = -2.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for QR operation in ENode::compute_local_cost");
        }
        case LU:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                if (rows != cols)
                    throw std::invalid_argument("Non-square matrix for LU");
                return (2.0 / 3.0) * rows * rows * rows;
            }
            if (is_symbolic(shape))
            {
                std::string n = size_to_symbol(shape.first);

                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 2.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for LU operation in ENode::compute_local_cost");
        }
        case LLt:
        {
            auto shape = get_one_shape(children.at(0));
            if (is_concrete(shape))
            {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                if (rows != cols)
                    throw std::invalid_argument("Non-square matrix for LLt");
                return (1.0 / 3.0) * rows * rows * rows;
            }
            if (is_symbolic(shape))
            {
                std::string n = size_to_symbol(shape.first);

                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 1.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for LLt operation in ENode::compute_local_cost");
        }
        case Get:
            return 0.0;
        case Sol:
            return 5.0;
        case TriSol:
            return 3.0;
        case Det:
            return 5.0;
        case Log:
            return 1.0;
        default:
            throw std::invalid_argument("Unknown Op in ENode::compute_local_cost");
        }
    }
    throw std::invalid_argument("ENode with non-Op atom should not have children");
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