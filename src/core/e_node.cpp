#include "e_node.h"
#include "basic_types.h"
#include "e_graph.h"
#include "op_registry.h"
#include "types.h"
#include "utils.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>

namespace egraph {
const Children &ENode::get_children() const { return children; }
Children &ENode::get_children_mut() { return children; }

Atom ENode::get_atom() const { return atom; }

std::string ENode::to_string() const { return atom_to_string(atom); }

std::string ENode::format() const {
    if (is_leaf()) {
        return to_string();
    }

    std::string str = "(" + to_string();
    std::ranges::for_each(children, [&](Id child_id) {
        str += " " + std::to_string(child_id);
    });
    str += ')';
    return str;
}

size_t ENode::hash() const {
    size_t seed;
    if (std::holds_alternative<Op>(atom)) {
        Op op = std::get<Op>(atom);
        seed = std::hash<std::underlying_type_t<Op>>()(static_cast<std::underlying_type_t<Op>>(op));
    } else if (std::holds_alternative<uint32_t>(atom)) {
        // use a fixed discriminant for string payloads;
        seed = std::hash<int>()(-1);
    } else {
        // double payload
        seed = std::hash<int>()(-2);
    }

    seed = std::accumulate(children.begin(), children.end(), seed, [](size_t acc, Id c) {
        size_t hc = std::hash<Id>()(c);
        return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
    });

    // if string payloads, mix in the string hash
    if (std::holds_alternative<uint32_t>(atom)) {
        const auto &s = std::get<uint32_t>(atom);
        size_t hp = std::hash<uint32_t>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    } else if (std::holds_alternative<int>(atom)) {
        int v = std::get<int>(atom);
        size_t hp = std::hash<int>()(v);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    } else if (std::holds_alternative<ScalarExpr>(atom)) {
        size_t hp = std::hash<std::string>()(std::get<ScalarExpr>(atom).to_string());
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const { return children.empty(); }

Cost ENode::compute_local_cost(const EGraph &egraph, const SizeBindings &size_bindings) const {
    if (is_leaf()) {
        return 0.0;
    }

    auto op = std::get_if<Op>(&atom);
    if (!op) {
        throw std::invalid_argument("ENode with non-Op atom should not have children");
    }

    const auto &desc = OpRegistry::instance().get(*op);
    if (!desc.cost) {
        throw std::invalid_argument("Operation has no cost function registered: " + desc.name);
    }

    std::vector<Operand> operands;
    operands.reserve(children.size());

    for (Id child_id : children) {
        if (auto idx = get_int_from_eclass(egraph, child_id)) {
            operands.emplace_back(*idx);
        } else if (auto dbl = get_double_from_eclass(egraph, child_id)) {
            operands.emplace_back(*dbl);
        } else {
            bool is_symbolic_scalar = false;
            for (const auto &cnode : egraph.get_class_nodes(child_id)) {
                if (std::holds_alternative<ScalarExpr>(cnode->get_atom())) {
                    is_symbolic_scalar = true;
                    break;
                }
            }
            if (is_symbolic_scalar) {
                operands.emplace_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            const auto &class_data = egraph.get_class_analysis_data(child_id);
            if (auto prop = std::get_if<MatrixProperty>(&class_data.property)) {
                MatrixOperand mat;
                mat.property = *prop;
                Shape bound = bind_shape(prop->shape, size_bindings);
                if (!is_numeric(bound)) {
                    throw std::runtime_error("Cannot compute cost: missing size binding for symbolic dimension");
                }
                mat.rows = std::get<int>(bound.first);
                mat.cols = std::get<int>(bound.second);
                mat.property.shape = bound;
                operands.emplace_back(mat);
            } else if (auto tuple_prop = std::get_if<TupleProperty>(&class_data.property)) {
                TupleOperand tuple;
                tuple.reserve(tuple_prop->size());
                for (const auto &elem_prop : *tuple_prop) {
                    MatrixOperand elem;
                    elem.property = elem_prop;
                    Shape bound = bind_shape(elem_prop.shape, size_bindings);
                    if (!is_numeric(bound)) {
                        throw std::runtime_error("Cannot compute cost: missing size binding for symbolic dimension");
                    }
                    elem.rows = std::get<int>(bound.first);
                    elem.cols = std::get<int>(bound.second);
                    elem.property.shape = bound;
                    tuple.push_back(elem);
                }
                operands.emplace_back(tuple);
            }
        }
    }

    return desc.cost(*op, operands);
}

} // namespace egraph
