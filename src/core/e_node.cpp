#include "e_node.h"
#include "e_graph.h"
#include "types.h"
#include "utils.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {
Size bind_size(const Size &size, const SizeBindings *size_bindings) {
    if (!size_bindings) {
        return size;
    }

    if (const auto *symbol = std::get_if<std::string>(&size)) {
        if (auto it = size_bindings->find(*symbol); it != size_bindings->end()) {
            return it->second;
        }
    }

    return size;
}

Shape bind_shape(const Shape &shape, const SizeBindings *size_bindings) {
    return {bind_size(shape.first, size_bindings), bind_size(shape.second, size_bindings)};
}

std::string size_to_symbol(const Size &size) {
    if (const auto *value = std::get_if<int>(&size)) {
        return std::to_string(*value);
    }
    return std::get<std::string>(size);
}
} // namespace

Cost ENode::compute_local_cost(const EGraph &egraph, const SizeBindings *size_bindings) const {
    auto get_one_shape = [&](Id child_id) -> Shape {
        auto data = get_matrix_data(egraph, child_id);
        if (data) {
            return bind_shape(data->shape, size_bindings);
        }
        return {{}, {}};
    };
    auto get_two_shapes = [&](Id child_id1, Id child_id2) -> std::pair<Shape, Shape> {
        auto shape1 = get_one_shape(child_id1);
        auto shape2 = get_one_shape(child_id2);
        return {shape1, shape2};
    };
    if (is_leaf()) {
        return 0.0;
    }
    if (auto op = std::get_if<Op>(&atom)) {
        switch (*op) {
            using enum Op;
        case Add: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                return static_cast<double>(rows * cols);
            }
            if (!is_numeric(shape)) {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Add operation in ENode::compute_local_cost");
        }
        case Mul: {
            auto shapes = get_two_shapes(children.at(0), children.at(1));
            if (is_numeric(shapes.first) && is_numeric(shapes.second)) {
                int rows1 = std::get<int>(shapes.first.first);
                int cols1 = std::get<int>(shapes.first.second);
                int cols2 = std::get<int>(shapes.second.second);
                return 2.0 * rows1 * cols1 * cols2;
            }
            if (!(is_numeric(shapes.first) && is_numeric(shapes.second))) {
                Monomial m = {
                    {size_to_symbol(shapes.first.first), size_to_symbol(shapes.first.second),
                     size_to_symbol(shapes.second.second)}};
                SymbolicCost sc;
                sc[m] = 2.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shapes for Mul operation in ENode::compute_local_cost");
        }
        case Tr: {
            auto shape = get_one_shape(children.at(0));
            return 0.0;
        }
        // LU L-1 then Solve
        case Inv: {
            auto shape = get_one_shape(children.at(0));
            auto data = get_matrix_data(egraph, egraph.find_node_id(*this).value());
            if (is_numeric(shape)) {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                if (rows != cols) {
                    throw std::invalid_argument(
                        "Non-square matrix for Inv operation in "
                        "ENode::compute_local_cost");
                }
                if (data && (data->flags.is_upper_triangular || data->flags.is_lower_triangular)) {
                    return (1.0 / 3.0) * rows * rows * rows;
                }
                return 8.0 * rows * rows * rows;
            }
            if (!is_numeric(shape)) {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.first), size_to_symbol(shape.first)}};
                SymbolicCost sc;
                if (data && (data->flags.is_upper_triangular || data->flags.is_lower_triangular)) {
                    sc[m] = 1.0 / 3.0;
                } else {
                    sc[m] = 8.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Inv operation in ENode::compute_local_cost");
        };
        case Neg: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                return static_cast<double>(rows * cols);
            }
            if (!is_numeric(shape)) {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
                SymbolicCost sc;
                sc[m] = 1.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Neg operation in ENode::compute_local_cost");
        };
        case QR: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                auto min_dim = std::min(rows, cols);
                auto max_dim = std::max(rows, cols);
                return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
            }
            if (!is_numeric(shape)) {
                std::string r = size_to_symbol(shape.first);
                std::string c = size_to_symbol(shape.second);
                if (auto data = get_matrix_data(egraph, egraph.find_node_id(*this).value())) {
                    if (data->flags.is_tall) {
                        r = size_to_symbol(shape.second);
                        c = size_to_symbol(shape.first);
                    }
                }
                Monomial mn2 = {{r, c, c}};
                Monomial n3 = {{c, c, c}};

                SymbolicCost sc;
                sc[mn2] = 2.0;
                sc[n3] = -2.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for QR operation in ENode::compute_local_cost");
        }
        case LU: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                if (rows != cols)
                    throw std::invalid_argument("Non-square matrix for LU");
                return (2.0 / 3.0) * rows * rows * rows;
            }
            if (!is_numeric(shape)) {
                std::string n = size_to_symbol(shape.first);

                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 2.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for LU operation in ENode::compute_local_cost");
        }
        case LLt: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                if (rows != cols)
                    throw std::invalid_argument("Non-square matrix for LLt");
                return (1.0 / 3.0) * rows * rows * rows;
            }
            if (!is_numeric(shape)) {
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
        // Ops below are not implemented
        case Sol: {
            auto shapeA = get_one_shape(children.at(0));
            auto shapeB = get_one_shape(children.at(1));
            
            bool is_triang = false;
            if (auto dataA = get_matrix_data(egraph, children.at(0))) {
                is_triang = dataA->flags.is_lower_triangular || dataA->flags.is_upper_triangular || dataA->flags.is_diagonal;
            }

            if (is_numeric(shapeA) && is_numeric(shapeB)) {
                double n = std::get<int>(shapeA.first);
                double k = std::get<int>(shapeB.second);

                if (is_triang) {
                    return 1.0 * n * n * k;
                }
                // LU Factorization (2/3 n^3) + Forward/Back Substitution (2 n^2 k)
                return (2.0 / 3.0) * n * n * n + 2.0 * n * n * k;
            }

            if (!is_numeric(shapeA) && !is_numeric(shapeB)) {
                std::string n = size_to_symbol(shapeA.first);
                std::string k = size_to_symbol(shapeB.second);

                Monomial n3 = {{n, n, n}};
                Monomial n2k = {{n, n, k}};

                SymbolicCost sc;
                if (is_triang) {
                    sc[n2k] = 1.0;
                } else {
                    sc[n3] = 2.0 / 3.0;
                    sc[n2k] = 2.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid or mixed shapes for Sol operation in ENode::compute_local_cost");
        }

        case SolR: {
            auto shapeB = get_one_shape(children.at(0));
            auto shapeA = get_one_shape(children.at(1));
            
            bool is_triang = false;
            if (auto dataA = get_matrix_data(egraph, children.at(1))) {
                is_triang = dataA->flags.is_lower_triangular || dataA->flags.is_upper_triangular || dataA->flags.is_diagonal;
            }

            if (is_numeric(shapeB) && is_numeric(shapeA)) {
                double m = std::get<int>(shapeB.first);
                double n = std::get<int>(shapeA.first);

                if (is_triang) {
                    return 1.0 * m * n * n;
                }
                // LU Factorization (2/3 n^3) + Forward/Back Substitution (2 m n^2)
                return (2.0 / 3.0) * n * n * n + 2.0 * m * n * n;
            }

            if (!is_numeric(shapeB) && !is_numeric(shapeA)) {
                std::string m = size_to_symbol(shapeB.first);
                std::string n = size_to_symbol(shapeA.first);

                Monomial n3 = {{n, n, n}};
                Monomial mn2 = {{m, n, n}};

                SymbolicCost sc;
                if (is_triang) {
                    sc[mn2] = 1.0;
                } else {
                    sc[n3] = 2.0 / 3.0;
                    sc[mn2] = 2.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid or mixed shapes for SolR operation in ENode::compute_local_cost");
        }
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

const Children &ENode::get_children() const { return children; }
Children &ENode::get_children_mut() { return children; }

Atom ENode::get_atom() const { return atom; }

std::string ENode::to_string() const {
    if (std::holds_alternative<Op>(atom)) {
        Op op = std::get<Op>(atom);
        switch (op) {
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
        case SolR:
            return "SolR";
        case Det:
            return "Det";
        case Log:
            return "Log";
        default:
            throw std::invalid_argument("Unknown Op in ENode::to_string");
        }
    } else if (std::holds_alternative<std::string>(atom)) {
        return std::get<std::string>(atom);
    } else if (std::holds_alternative<int>(atom)) {
        return std::to_string(std::get<int>(atom));
    }
    throw std::invalid_argument("Unknown atom type in ENode::to_string");
}

std::string ENode::format() const {
    if (is_leaf()) {
        return to_string();
    }

    std::string str = "(" + to_string();
    std::ranges::for_each(children, [&](Id child_id) {
        str += " " + std::to_string(child_id);
    });
    str += ")";
    return str;
}

size_t ENode::hash() const {
    size_t seed;
    if (std::holds_alternative<Op>(atom)) {
        Op op = std::get<Op>(atom);
        seed = std::hash<int>()(static_cast<std::underlying_type_t<Op>>(op));
    } else if (std::holds_alternative<std::string>(atom)) {
        // use a fixed discriminant for string payloads;
        seed = std::hash<int>()(-1);
    } else {
        // int payload
        seed = std::hash<int>()(-2);
    }

    seed = std::accumulate(children.begin(), children.end(), seed, [](size_t acc, Id c) {
        size_t hc = std::hash<Id>()(c);
        return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
    });

    // if string payloads, mix in the string hash
    if (std::holds_alternative<std::string>(atom)) {
        const auto &s = std::get<std::string>(atom);
        size_t hp = std::hash<std::string>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    } else if (std::holds_alternative<int>(atom)) {
        int i = std::get<int>(atom);
        size_t hp = std::hash<int>()(i);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const { return children.empty(); }

static bool has_ancestor_impl(
    const ENode &node, std::string_view ancestor_op, const EGraph &egraph, std::unordered_set<Id> &visited) {
    if (std::holds_alternative<Op>(node.get_atom()) && node.to_string() == ancestor_op) {
        return true;
    }
    auto opt_id = egraph.find_node_id(node);
    if (!opt_id.has_value())
        return false;

    Id this_class_id = egraph.find_class_id(opt_id.value());
    if (!visited.insert(this_class_id).second) {
        return false;
    }

    auto parent_ids = egraph.get_class_parents(this_class_id);
    for (Id parent_id : parent_ids) {
        const ENode &parent_node = egraph.at(parent_id);
        if (has_ancestor_impl(parent_node, ancestor_op, egraph, visited)) {
            return true;
        }
    }
    return false;
}

bool ENode::has_ancestor(std::string_view ancestor_op, const EGraph &egraph) const {
    std::unordered_set<Id> visited;
    return has_ancestor_impl(*this, ancestor_op, egraph, visited);
}