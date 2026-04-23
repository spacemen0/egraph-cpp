#include "expression.h"
#include "basic_types.h"
#include "utils.h"
#include <algorithm>
#include <sstream>

namespace {
int precedence(const Expression &expr) {
    if (!std::holds_alternative<Op>(expr.atom)) {
        return 100;
    }

    switch (std::get<Op>(expr.atom)) {
        using enum Op;
    case Add:
    case Minus:
        return 10;
    case Mul:
        return 20;
    case Tr:
    case Inv:
        return 40;
    default:
        return 90;
    }
}

std::string render(const Expression &expr, bool human_readable, int parent_precedence = 0);

std::string parenthesize(const Expression &expr, bool human_readable, int parent_precedence) {
    std::string rendered = render(expr, human_readable, parent_precedence);
    if (precedence(expr) < parent_precedence) {
        return "(" + rendered + ")";
    }
    return rendered;
}

std::string parenthesize_mul_chain(const Expression &expr, bool human_readable) {
    constexpr int MulPrecedence = 20;
    std::string rendered = render(expr, human_readable, MulPrecedence);
    if (std::holds_alternative<Op>(expr.atom) && std::get<Op>(expr.atom) == Op::Mul) {
        return "(" + rendered + ")";
    }
    if (precedence(expr) < MulPrecedence) {
        return "(" + rendered + ")";
    }
    return rendered;
}

std::string format_indexed_factorization(const Expression &base, const Expression &index_expr) {
    if (!std::holds_alternative<int>(index_expr.atom)) {
        return {};
    }

    const int index = std::get<int>(index_expr.atom);
    if (!std::holds_alternative<Op>(base.atom)) {
        return {};
    }

    const auto *inner = base.children.empty() ? nullptr : &base.children[0];
    const std::string inner_str = inner ? render(*inner, true) : "?";

    switch (std::get<Op>(base.atom)) {
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
            return "Chol(" + inner_str + ")";
        break;
    default:
        break;
    }

    return {};
}

std::string render(const Expression &expr, bool human_readable, int parent_precedence) {
    if (!std::holds_alternative<Op>(expr.atom)) {
        return atom_to_string(expr.atom);
    }

    const Op op = std::get<Op>(expr.atom);
    switch (op) {
        using enum Op;
    case Add: {
        if (expr.children.empty())
            return "0";
        std::stringstream ss;
        for (size_t i = 0; i < expr.children.size(); ++i) {
            if (i > 0)
                ss << " + ";
            ss << parenthesize(expr.children[i], human_readable, precedence(expr));
        }
        return ss.str();
    }
    case Mul: {
        if (expr.children.empty())
            return "1";
        std::stringstream ss;
        for (size_t i = 0; i < expr.children.size(); ++i) {
            if (i > 0)
                ss << " * ";
            ss << parenthesize_mul_chain(expr.children[i], human_readable);
        }
        return ss.str();
    }
    case Minus: {
        if (expr.children.size() < 2)
            return "? - ?";
        return parenthesize(expr.children[0], human_readable, precedence(expr)) + " - " +
               parenthesize(expr.children[1], human_readable, precedence(expr));
    }
    case Tr: {
        if (expr.children.empty())
            return human_readable ? "(?)ᵀ" : "Tr(?)";
        const Expression &arg = expr.children[0];
        if (human_readable) {
            const std::string arg_str =
                precedence(arg) < precedence(expr) ? "(" + render(arg, true) + ")" : render(arg, true);
            return arg_str + "ᵀ";
        } else {
            return "Tr(" + render(arg, false) + ")";
        }
    }
    case Inv: {
        if (expr.children.empty())
            return human_readable ? "(?)⁻¹" : "Inv(?)";
        const Expression &arg = expr.children[0];
        if (human_readable) {
            const std::string arg_str =
                precedence(arg) < precedence(expr) ? "(" + render(arg, true) + ")" : render(arg, true);
            return arg_str + "⁻¹";
        } else {
            return "Inv(" + render(arg, false) + ")";
        }
    }
    case Get: {
        if (expr.children.size() >= 2) {
            if (human_readable) {
                const std::string decomposition_view = format_indexed_factorization(expr.children[0], expr.children[1]);
                if (!decomposition_view.empty()) {
                    return decomposition_view;
                }
            }
            return render(expr.children[0], human_readable) + "[" + render(expr.children[1], human_readable) + "]";
        }
        break;
    }
    case Det:
        if (!expr.children.empty())
            return "Det(" + render(expr.children[0], human_readable) + ")";
        break;
    case Log:
        if (!expr.children.empty())
            return "Log(" + render(expr.children[0], human_readable) + ")";
        break;
    case Sol:
        if (expr.children.size() == 2) {
            std::string name = human_readable ? "Solve" : "Sol";
            return name + "(" + render(expr.children[0], human_readable) + ", " +
                   render(expr.children[1], human_readable) + ")";
        }
        break;
    case SolR:
        if (expr.children.size() == 2) {
            std::string name = human_readable ? "Solve_Right" : "SolR";
            return name + "(" + render(expr.children[0], human_readable) + ", " +
                   render(expr.children[1], human_readable) + ")";
        }
        break;
    case QR:
    case LU:
    case LLt: {
        std::stringstream ss;
        ss << atom_to_string(expr.atom) << "(";
        for (size_t i = 0; i < expr.children.size(); ++i) {
            if (i > 0)
                ss << ", ";
            ss << render(expr.children[i], human_readable);
        }
        ss << ")";
        return ss.str();
    }
    }

    // Fallback
    std::stringstream ss;
    ss << atom_to_string(expr.atom) << "(";
    for (size_t i = 0; i < expr.children.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << render(expr.children[i], human_readable);
    }
    ss << ")";
    return ss.str();
}
} // namespace

/// @brief parse a expression from a string with the format: Op(child1, child2,
/// ...)
/// @param string
Expression::Expression(std::string_view string) {
    auto parsed = string_to_parsed_atom(string);
    atom = parsed.atom;
    std::ranges::transform(parsed.children_strings, std::back_inserter(children), [](const std::string &str) {
        return Expression(str);
    });
}

Expression::Expression(const ENode &node, const EGraph &egraph) : atom(node.get_atom()) {
    const auto &child_ids = node.get_children();
    children.reserve(child_ids.size());
    for (Id child_id : child_ids) {
        const auto &child_nodes = egraph.get_class_nodes(child_id);
        if (child_nodes.empty()) {
            throw std::runtime_error("No nodes in class for child id: " + std::to_string(child_id));
        }
        children.emplace_back(*child_nodes[0], egraph);
    }
}

std::string Expression::to_string() const {
    return render(*this, false);
}

std::string Expression::to_human_string() const {
    return render(*this, true);
}

bool Expression::operator==(const Expression &other) const {
    if (atom != other.atom)
        return false;
    if (children.size() != other.children.size())
        return false;
    for (size_t i = 0; i < children.size(); ++i) {
        if (!(children[i] == other.children[i]))
            return false;
    }
    return true;
}
