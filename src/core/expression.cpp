#include <stdexcept>
#include <sstream>
#include <algorithm>
#include "expression.h"
#include "utils.h"

namespace
{
    int precedence(const Expression &expr)
    {
        if (!std::holds_alternative<Op>(expr.atom))
        {
            return 100;
        }

        switch (std::get<Op>(expr.atom))
        {
            using enum Op;
        case Add:
            return 10;
        case Mul:
            return 20;
        case Neg:
            return 30;
        case Tr:
        case Inv:
            return 40;
        default:
            return 90;
        }
    }

    std::string parenthesize_if_needed(const Expression &expr, int parent_precedence)
    {
        std::string rendered = expr.to_human_string();
        if (precedence(expr) < parent_precedence)
        {
            return "(" + rendered + ")";
        }
        return rendered;
    }

    std::string format_indexed_factorization(const Expression &base, const Expression &index_expr)
    {
        if (!std::holds_alternative<int>(index_expr.atom))
        {
            return {};
        }

        const int index = std::get<int>(index_expr.atom);
        if (!std::holds_alternative<Op>(base.atom))
        {
            return {};
        }

        const auto *inner = base.children.empty() ? nullptr : &base.children[0];
        const std::string inner_str = inner ? inner->to_human_string() : "?";

        switch (std::get<Op>(base.atom))
        {
            using enum Op;
        case QR:
            if (index == 0)
                return "Q(" + inner_str + ")";
            if (index == 1)
                return "R(" + inner_str + ")";
            break;
        case LU:
            if (index == 0)
                return "L(" + inner_str + ")";
            if (index == 1)
                return "U(" + inner_str + ")";
            if (index == 2)
                return "P(" + inner_str + ")";
            break;
        case LLt:
            if (index == 0)
                return "L(" + inner_str + ")";
            break;
        default:
            break;
        }

        return {};
    }
} // namespace

/// @brief parse a expression from a string with the format: Op(child1, child2, ...)
/// @param string
Expression::Expression(std::string_view string)
{
    auto parsed = string_to_parsed_atom(string);
    atom = parsed.atom;
    std::ranges::transform(parsed.children_strings, std::back_inserter(children),
                           [](const std::string &str)
                           {
                               return Expression(str);
                           });
}

std::string Expression::to_string() const
{
    std::stringstream ss;
    std::string atom_str = atom_to_string(this->atom);

    if (children.empty())
    {
        return atom_str;
    }

    ss << atom_str << "(";
    for (size_t i = 0; i < children.size(); ++i)
    {
        ss << children[i].to_string();
        if (i < children.size() - 1)
        {
            ss << ", ";
        }
    }
    ss << ")";
    return ss.str();
}

std::string Expression::to_human_string() const
{
    if (!std::holds_alternative<Op>(atom))
    {
        return atom_to_string(atom);
    }

    const Op op = std::get<Op>(atom);
    switch (op)
    {
        using enum Op;
    case Add:
    {
        if (children.empty())
            return "0";
        std::stringstream ss;
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (i > 0)
                ss << " + ";
            ss << parenthesize_if_needed(children[i], precedence(*this));
        }
        return ss.str();
    }
    case Mul:
    {
        if (children.empty())
            return "1";
        std::stringstream ss;
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (i > 0)
                ss << " * ";
            ss << parenthesize_if_needed(children[i], precedence(*this));
        }
        return ss.str();
    }
    case Neg:
    {
        if (children.empty())
            return "-?";
        return "-" + parenthesize_if_needed(children[0], precedence(*this));
    }
    case Tr:
    {
        if (children.empty())
            return "(?)ᵀ";
        const Expression &arg = children[0];
        const std::string arg_str = precedence(arg) < precedence(*this) ? "(" + arg.to_human_string() + ")" : arg.to_human_string();
        return arg_str + "ᵀ";
    }
    case Inv:
    {
        if (children.empty())
            return "(?)⁻¹";
        const Expression &arg = children[0];
        const std::string arg_str = precedence(arg) < precedence(*this) ? "(" + arg.to_human_string() + ")" : arg.to_human_string();
        return arg_str + "⁻¹";
    }
    case Get:
    {
        if (children.size() >= 2)
        {
            const std::string decomposition_view = format_indexed_factorization(children[0], children[1]);
            if (!decomposition_view.empty())
            {
                return decomposition_view;
            }
            return children[0].to_human_string() + "[" + children[1].to_human_string() + "]";
        }
        break;
    }
    case Det:
        if (!children.empty())
            return "det(" + children[0].to_human_string() + ")";
        break;
    case Log:
        if (!children.empty())
            return "log(" + children[0].to_human_string() + ")";
        break;
    case Sol:
        if (children.size() == 2)
            return "solve(" + children[0].to_human_string() + ", " + children[1].to_human_string() + ")";
        break;
    case TriSol:
        if (children.size() == 2)
            return "tri_solve(" + children[0].to_human_string() + ", " + children[1].to_human_string() + ")";
        break;
    case QR:
    case LU:
    case LLt:
    {
        std::stringstream ss;
        ss << atom_to_string(atom) << "(";
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (i > 0)
                ss << ", ";
            ss << children[i].to_human_string();
        }
        ss << ")";
        return ss.str();
    }
    }

    // Fallback to canonical form if we do not have a custom pretty renderer.
    return to_string();
}

bool Expression::operator==(const Expression &other) const
{
    if (atom != other.atom)
        return false;
    if (children.size() != other.children.size())
        return false;
    for (size_t i = 0; i < children.size(); ++i)
    {
        if (!(children[i] == other.children[i]))
            return false;
    }
    return true;
}
