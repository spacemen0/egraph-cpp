#include "e_node.h"
#include "basic_types.h"
#include "e_graph.h"
#include "types.h"
#include "utils.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>

namespace {

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
            return 0.0;
        }
        // LU L-1 then Solve
        case Inv: {
            auto shape = get_one_shape(children.at(0));
            auto data = get_matrix_data(egraph, children.at(0)); // Get from child
            if (is_numeric(shape)) {
                int rows = std::get<int>(shape.first);
                int cols = std::get<int>(shape.second);
                if (rows != cols) {
                    throw std::invalid_argument(
                        "Non-square matrix for Inv operation in "
                        "ENode::compute_local_cost");
                }
                if (data &&
                    (data->flags.is_upper_triangular || data->flags.is_lower_triangular || data->flags.is_diagonal)) {
                    return (1.0 / 3.0) * rows * rows * rows;
                }
                return 8.0 * rows * rows * rows;
            }
            if (!is_numeric(shape)) {
                Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.first), size_to_symbol(shape.first)}};
                SymbolicCost sc;
                if (data &&
                    (data->flags.is_upper_triangular || data->flags.is_lower_triangular || data->flags.is_diagonal)) {
                    sc[m] = 1.0 / 3.0;
                } else {
                    sc[m] = 8.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid shape for Inv operation in ENode::compute_local_cost");
        };
        case Minus: {
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
            throw std::invalid_argument("Invalid shape for Minus operation in ENode::compute_local_cost");
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
                if (auto data = get_matrix_data(egraph, children.at(0))) {
                    if (data->is_wide_matrix()) {
                        std::swap(r, c);
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
        case UtU: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                if (rows != cols)
                    throw std::invalid_argument("Non-square matrix for UtU");
                return (1.0 / 3.0) * rows * rows * rows;
            }
            if (!is_numeric(shape)) {
                std::string n = size_to_symbol(shape.first);

                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 1.0 / 3.0;
                return sc;
            }
            throw std::invalid_argument("Invalid shape for UtU operation in ENode::compute_local_cost");
        }
        case Get:
            return 0.0;
        case Sol: {
            auto shapeA = get_one_shape(children.at(0));
            auto shapeB = get_one_shape(children.at(1));

            bool is_triangular = false;
            if (auto dataA = get_matrix_data(egraph, children.at(0))) {
                is_triangular =
                    dataA->flags.is_lower_triangular || dataA->flags.is_upper_triangular || dataA->flags.is_diagonal;
            }

            if (is_numeric(shapeA) && is_numeric(shapeB)) {
                double n = std::get<int>(shapeA.first);
                double k = std::get<int>(shapeB.second);

                if (is_triangular) {
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
                if (is_triangular) {
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
            auto shapeA = get_one_shape(children.at(0));
            auto shapeB = get_one_shape(children.at(1));

            bool is_triangular = false;
            if (auto dataA = get_matrix_data(egraph, children.at(0))) {
                is_triangular =
                    dataA->flags.is_lower_triangular || dataA->flags.is_upper_triangular || dataA->flags.is_diagonal;
            }

            if (is_numeric(shapeA) && is_numeric(shapeB)) {
                double n = std::get<int>(shapeA.first);
                double m = std::get<int>(shapeB.first);

                if (is_triangular) {
                    return 1.0 * n * n * m;
                }
                // LU Factorization (2/3 n^3) + Forward/Back Substitution (2 n^2 m)
                return (2.0 / 3.0) * n * n * n + 2.0 * n * n * m;
            }

            if (!is_numeric(shapeA) && !is_numeric(shapeB)) {
                std::string n = size_to_symbol(shapeA.first);
                std::string m = size_to_symbol(shapeB.first);

                Monomial n3 = {{n, n, n}};
                Monomial n2m = {{n, n, m}};

                SymbolicCost sc;
                if (is_triangular) {
                    sc[n2m] = 1.0;
                } else {
                    sc[n3] = 2.0 / 3.0;
                    sc[n2m] = 2.0;
                }
                return sc;
            }
            throw std::invalid_argument("Invalid or mixed shapes for SolR operation in ENode::compute_local_cost");
        }
        case Scale:
            return 0.0;
        case Det:
            return 5.0;
        case Log:
            return 1.0;

        case Gemm_NN:
        case Gemm_TN:
        case Gemm_NT:
        case Gemm_TT: {
            auto shapeA = get_one_shape(children.at(0));
            auto shapeB = get_one_shape(children.at(1));
            Size M, K, N;
            if (*op == Gemm_NN) {
                M = shapeA.first;
                K = shapeA.second;
                N = shapeB.second;
            } else if (*op == Gemm_TN) {
                M = shapeA.second;
                K = shapeA.first;
                N = shapeB.second;
            } else if (*op == Gemm_NT) {
                M = shapeA.first;
                K = shapeA.second;
                N = shapeB.first;
            } else { // Gemm_TT
                M = shapeA.second;
                K = shapeA.first;
                N = shapeB.first;
            }

            if (is_numeric(shapeA) && is_numeric(shapeB)) {
                int m_val = std::get<int>(M);
                int k_val = std::get<int>(K);
                int n_val = std::get<int>(N);
                return 2.0 * m_val * k_val * n_val;
            } else {
                Monomial m = {{size_to_symbol(M), size_to_symbol(K), size_to_symbol(N)}};
                SymbolicCost sc;
                sc[m] = 2.0;
                return sc;
            }
        }

        case Syrk_N:
        case Syrk_T: {
            auto shape = get_one_shape(children.at(0));
            Size N_dim = (*op == Syrk_N) ? shape.first : shape.second;
            Size K_dim = (*op == Syrk_N) ? shape.second : shape.first;

            if (is_numeric(shape)) {
                int n_val = std::get<int>(N_dim);
                int k_val = std::get<int>(K_dim);
                return 1.0 * n_val * (n_val + 1.0) * k_val;
            } else {
                Monomial n2k = {{size_to_symbol(N_dim), size_to_symbol(N_dim), size_to_symbol(K_dim)}};
                Monomial nk = {{size_to_symbol(N_dim), size_to_symbol(K_dim)}};
                SymbolicCost sc;
                sc[n2k] = 1.0;
                sc[nk] = 1.0;
                return sc;
            }
        }

        case Trsm_LN:
        case Trsm_LT:
        case Trsm_RN:
        case Trsm_RT: {
            auto shapeA = get_one_shape(children.at(0));
            auto shapeB = get_one_shape(children.at(1));
            Size M_dim, N_dim;

            bool is_left_solve = (*op == Trsm_LN || *op == Trsm_LT);

            if (is_left_solve) {
                M_dim = (*op == Trsm_LN) ? shapeA.first : shapeA.second;
                N_dim = shapeB.second;
            } else {
                M_dim = shapeB.first;
                N_dim = (*op == Trsm_RN) ? shapeA.second : shapeA.first;
            }

            if (is_numeric(shapeA) && is_numeric(shapeB)) {
                int m_val = std::get<int>(M_dim);
                int n_val = std::get<int>(N_dim);

                // Branch the calculation based on solve side
                if (is_left_solve) {
                    return 1.0 * m_val * m_val * n_val; // M^2 * N
                } else {
                    return 1.0 * m_val * n_val * n_val; // M * N^2
                }
            } else {
                SymbolicCost sc;
                if (is_left_solve) {
                    Monomial m = {{size_to_symbol(M_dim), size_to_symbol(M_dim), size_to_symbol(N_dim)}};
                    sc[m] = 1.0;
                } else {
                    Monomial m = {{size_to_symbol(M_dim), size_to_symbol(N_dim), size_to_symbol(N_dim)}};
                    sc[m] = 1.0;
                }
                return sc;
            }
        }
        case Potrf_L:
        case Potrf_U: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                return (1.0 / 3.0) * rows * rows * rows;
            } else {
                std::string n = size_to_symbol(shape.first);
                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 1.0 / 3.0;
                return sc;
            }
        }
        case Geqrf: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                double cols = std::get<int>(shape.second);
                auto min_dim = std::min(rows, cols);
                auto max_dim = std::max(rows, cols);
                return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
            } else {
                std::string r = size_to_symbol(shape.first);
                std::string c = size_to_symbol(shape.second);
                if (auto data = get_matrix_data(egraph, children.at(0))) {
                    if (data->is_wide_matrix()) {
                        std::swap(r, c);
                    }
                }
                Monomial mn2 = {{r, c, c}};
                Monomial n3 = {{c, c, c}};

                SymbolicCost sc;
                sc[mn2] = 2.0;
                sc[n3] = -2.0 / 3.0;
                return sc;
            }
        }
        case Trtri: {
            auto shape = get_one_shape(children.at(0));
            if (is_numeric(shape)) {
                double rows = std::get<int>(shape.first);
                return (1.0 / 3.0) * rows * rows * rows;
            } else {
                std::string n = size_to_symbol(shape.first);
                Monomial n3 = {{n, n, n}};
                SymbolicCost sc;
                sc[n3] = 1.0 / 3.0;
                return sc;
            }
        }
        case Gemv_N: {
            auto shapes = get_two_shapes(children.at(0), children.at(1));
            if (is_numeric(shapes.first)) {
                int rows = std::get<int>(shapes.first.first);
                int cols = std::get<int>(shapes.first.second);
                return 2.0 * rows * cols;
            } else {
                Monomial m = {{size_to_symbol(shapes.first.first), size_to_symbol(shapes.first.second)}};
                SymbolicCost sc;
                sc[m] = 2.0;
                return sc;
            }
        }
        case Gemv_T: {
            auto shapes = get_two_shapes(children.at(0), children.at(1));
            if (is_numeric(shapes.first)) {
                int rows = std::get<int>(shapes.first.first);
                int cols = std::get<int>(shapes.first.second);
                return 2.0 * rows * cols;
            } else {
                Monomial m = {{size_to_symbol(shapes.first.first), size_to_symbol(shapes.first.second)}};
                SymbolicCost sc;
                sc[m] = 2.0;
                return sc;
            }
        }
        default:
            throw std::invalid_argument("Unknown Op in ENode::compute_local_cost");
        }
    }
    throw std::invalid_argument("ENode with non-Op atom should not have children");
}

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
    str += ")";
    return str;
}

size_t ENode::hash() const {
    size_t seed;
    if (std::holds_alternative<Op>(atom)) {
        Op op = std::get<Op>(atom);
        seed = std::hash<int>()(static_cast<std::underlying_type_t<Op>>(op));
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
    } else if (std::holds_alternative<double>(atom)) {
        double d = std::get<double>(atom);
        size_t hp = std::hash<double>()(d);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const { return children.empty(); }

static LookupTable &get_lookup_table() {
    static LookupTable table;
    return table;
}

static std::vector<std::string> &get_reverse_lookup() {
    static std::vector<std::string> reverse;
    return reverse;
}

std::string get_string_from_lookup(uint32_t id) {
    auto &reverse = get_reverse_lookup();
    if (id < reverse.size()) {
        return reverse[id];
    }
    return "<unknown>";
}

uint32_t register_string_in_lookup(const std::string &s) {
    auto &table = get_lookup_table();
    if (table.find(s) == table.end()) {
        uint32_t id = static_cast<uint32_t>(table.size());
        table[s] = id;
        get_reverse_lookup().push_back(s);
    }
    return table[s];
}
