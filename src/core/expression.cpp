#include "expression.h"
#include "basic_types.h"
#include "utils.h"
#include <algorithm>
#include <sstream>

namespace egraph {
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

std::string parenthesize(const Expression &expr, bool readable, int parent_precedence) {
    std::string rendered = Expression::render(expr, readable, parent_precedence);
    if (precedence(expr) < parent_precedence) {
        return "(" + rendered + ")";
    }
    return rendered;
}

std::string parenthesize_mul_chain(const Expression &expr, bool readable) {
    constexpr int MulPrecedence = 20;
    std::string rendered = Expression::render(expr, readable, MulPrecedence);
    if (std::holds_alternative<Op>(expr.atom) && std::get<Op>(expr.atom) == Op::Mul) {
        return "(" + rendered + ")";
    }
    if (precedence(expr) < MulPrecedence) {
        return "(" + rendered + ")";
    }
    return rendered;
}

std::string Expression::render(const Expression &expr, bool readable, int parent_precedence) {
    if (!std::holds_alternative<Op>(expr.atom)) {
        return atom_to_string(expr.atom);
    }

    const Op op = std::get<Op>(expr.atom);

    if (readable) {
        if (op == Op::Tr) {
            return Expression::render(expr.children[0], readable) + "ᵀ";
        }
        if (op == Op::Inv) {
            return Expression::render(expr.children[0], readable) + "⁻¹";
        }
    }

    switch (op) {
        using enum Op;
    case Add: {
        return parenthesize(expr.children[0], readable, precedence(expr)) + " + " +
               parenthesize(expr.children[1], readable, precedence(expr));
    }
    case Mul: {
        return parenthesize_mul_chain(expr.children[0], readable) + " * " +
               parenthesize_mul_chain(expr.children[1], readable);
    }
    case Minus: {
        return parenthesize(expr.children[0], readable, precedence(expr)) + " - " +
               parenthesize(expr.children[1], readable, precedence(expr));
    }
    case Get: {
        if (readable && expr.children.size() == 2) {
            if (const int *idx_ptr = std::get_if<int>(&expr.children[1].atom)) {
                int index = *idx_ptr;
                const Expression &tuple_expr = expr.children[0];
                if (std::holds_alternative<Op>(tuple_expr.atom)) {
                    Op tuple_op = std::get<Op>(tuple_expr.atom);
                    std::string factor_name;
                    if (tuple_op == Op::QR || tuple_op == Op::Geqrf) {
                        if (index == 0)
                            factor_name = "Q";
                        else if (index == 1)
                            factor_name = "R";
                    } else if (tuple_op == Op::LU) {
                        if (index == 0)
                            factor_name = "L";
                        else if (index == 1)
                            factor_name = "U";
                        else if (index == 2)
                            factor_name = "P";
                    } else if (tuple_op == Op::LLt || tuple_op == Op::Potrf_L || tuple_op == Op::Potrf_U) {
                        if (index == 0)
                            factor_name = "LLt";
                    }

                    if (!factor_name.empty()) {
                        std::stringstream ss;
                        ss << factor_name << "(";
                        for (size_t i = 0; i < tuple_expr.children.size(); ++i) {
                            if (i > 0)
                                ss << ", ";
                            ss << render(tuple_expr.children[i], readable);
                        }
                        ss << ")";
                        return ss.str();
                    }
                }
            }
        }
        break;
    }
    default:
        break;
    }

    // Fallback for all other operations (functional notation)
    std::stringstream ss;
    ss << atom_to_string(expr.atom) << "(";
    for (size_t i = 0; i < expr.children.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << render(expr.children[i], readable);
    }
    ss << ")";
    return ss.str();
}

/// @brief parse a expression from a string with the format: Op(child1, child2,
/// ...)
/// @param string
Expression::Expression(std::string_view string) {
    auto parsed = string_to_parsed_atom(string);
    atom = parsed.atom;
    if (std::holds_alternative<Op>(atom) && std::get<Op>(atom) == Op::Scale && parsed.children_strings.size() == 2) {
        children.push_back(Expression(parsed.children_strings[0]));
        children.push_back(Expression(parser::parse_scalar(parsed.children_strings[1])));
        return;
    }
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

std::string Expression::to_string(bool readable) const { return render(*this, readable); }

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

size_t Expression::depth() const {
    size_t max_child_depth = 0;
    for (const auto &child : children) {
        max_child_depth = std::max(max_child_depth, child.depth());
    }
    return 1 + max_child_depth;
}

size_t Expression::node_count() const {
    size_t count = 1;
    for (const auto &child : children) {
        count += child.node_count();
    }
    return count;
}

} // namespace egraph
