#include "op_costs.h"
#include "utils.h"
#include <algorithm>

namespace egraph {
using enum Op;

std::string size_to_symbol(const Size &size) {
    if (const auto *value = std::get_if<int>(&size)) {
        return std::to_string(*value);
    }
    return std::get<std::string>(size);
}

static Shape get_one_shape(const EGraph &egraph, const SizeBindings *size_bindings, Id child_id) {
    auto data = get_matrix_data(egraph, child_id);
    if (data) {
        return bind_shape(data->shape, size_bindings);
    }
    return {{}, {}};
}

static std::pair<Shape, Shape>
get_two_shapes(const EGraph &egraph, const SizeBindings *size_bindings, Id child_id1, Id child_id2) {
    auto shape1 = get_one_shape(egraph, size_bindings, child_id1);
    auto shape2 = get_one_shape(egraph, size_bindings, child_id2);
    return {shape1, shape2};
}

Cost compute_add_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        int rows = std::get<int>(shape.first);
        int cols = std::get<int>(shape.second);
        return static_cast<double>(rows * cols);
    } else {
        Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
        SymbolicCost sc;
        sc[m] = 1.0;
        return sc;
    }
}

static bool are_transposes(const EGraph &egraph, Id id1, Id id2) {
    Id root1 = egraph.find_class_id(id1);
    Id root2 = egraph.find_class_id(id2);
    for (const auto *n : egraph.get_class_nodes(root1)) {
        if (std::holds_alternative<Op>(n->get_atom()) && std::get<Op>(n->get_atom()) == Op::Tr) {
            if (egraph.find_class_id(n->get_children().at(0)) == root2) {
                return true;
            }
        }
    }
    for (const auto *n : egraph.get_class_nodes(root2)) {
        if (std::holds_alternative<Op>(n->get_atom()) && std::get<Op>(n->get_atom()) == Op::Tr) {
            if (egraph.find_class_id(n->get_children().at(0)) == root1) {
                return true;
            }
        }
    }
    if (root1 == root2) {
        if (const auto *prop = get_matrix_data(egraph, root1)) {
            if (prop->flags.is_symmetric) {
                return true;
            }
        }
    }
    return false;
}

Cost compute_mul_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapes = get_two_shapes(egraph, size_bindings, node.get_children().at(0), node.get_children().at(1));
    if (are_transposes(egraph, node.get_children().at(0), node.get_children().at(1))) {
        Size N_dim = shapes.first.first;
        Size K_dim = shapes.first.second;
        if (is_numeric(shapes.first)) {
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

    auto prop0 = get_matrix_data(egraph, node.get_children().at(0));
    auto prop1 = get_matrix_data(egraph, node.get_children().at(1));

    bool is_sq0 = prop0 && (prop0->is_square() || shapes.first.first == shapes.first.second);
    bool is_sq1 = prop1 && (prop1->is_square() || shapes.second.first == shapes.second.second);

    bool is_trmm_left = is_sq0 && (prop0->flags.is_upper_triangular || prop0->flags.is_lower_triangular);

    bool is_trmm_right = is_sq1 && (prop1->flags.is_upper_triangular || prop1->flags.is_lower_triangular);

    double coeff = (is_trmm_left || is_trmm_right) ? 1.0 : 2.0;

    if (is_numeric(shapes.first) && is_numeric(shapes.second)) {
        int rows1 = std::get<int>(shapes.first.first);
        int cols1 = std::get<int>(shapes.first.second);
        int cols2 = std::get<int>(shapes.second.second);
        return coeff * rows1 * cols1 * cols2;
    } else {
        Monomial m = {
            {size_to_symbol(shapes.first.first), size_to_symbol(shapes.first.second),
             size_to_symbol(shapes.second.second)}};
        SymbolicCost sc;
        sc[m] = coeff;
        return sc;
    }
}

Cost compute_tr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    // future: should occur as zero cost if consumed as kernel parameter
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        int rows = std::get<int>(shape.first);
        int cols = std::get<int>(shape.second);
        return static_cast<double>(rows * cols);
    } else {
        Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
        SymbolicCost sc;
        sc[m] = 1.0;
        return sc;
    }
}

Cost compute_inv_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    auto data = get_matrix_data(egraph, node.get_children().at(0));
    if (is_numeric(shape)) {
        int rows = std::get<int>(shape.first);
        if (data && (data->flags.is_upper_triangular || data->flags.is_lower_triangular || data->flags.is_diagonal)) {
            return (1.0 / 3.0) * rows * rows * rows;
        }
        return 8.0 * rows * rows * rows;
    } else {
        Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.first), size_to_symbol(shape.first)}};
        SymbolicCost sc;
        if (data && (data->flags.is_upper_triangular || data->flags.is_lower_triangular || data->flags.is_diagonal)) {
            sc[m] = 1.0 / 3.0;
        } else {
            sc[m] = 8.0;
        }
        return sc;
    }
}

Cost compute_minus_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        int rows = std::get<int>(shape.first);
        int cols = std::get<int>(shape.second);
        return static_cast<double>(rows * cols);
    } else {
        Monomial m = {{size_to_symbol(shape.first), size_to_symbol(shape.second)}};
        SymbolicCost sc;
        sc[m] = 1.0;
        return sc;
    }
}

Cost compute_qr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        double rows = std::get<int>(shape.first);
        double cols = std::get<int>(shape.second);
        auto min_dim = std::min(rows, cols);
        auto max_dim = std::max(rows, cols);
        return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
    } else {
        std::string r = size_to_symbol(shape.first);
        std::string c = size_to_symbol(shape.second);
        if (auto data = get_matrix_data(egraph, node.get_children().at(0))) {
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

Cost compute_lu_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        double rows = std::get<int>(shape.first);
        return (2.0 / 3.0) * rows * rows * rows;
    } else {
        std::string n = size_to_symbol(shape.first);

        Monomial n3 = {{n, n, n}};
        SymbolicCost sc;
        sc[n3] = 2.0 / 3.0;
        return sc;
    }
}

Cost compute_cholel_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
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

Cost compute_choleu_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
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

Cost compute_get_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    // If Get's first child is Geqrf, and index is 0, return infinite cost to force using Orgqr
    Id child_id = node.get_children().at(0);
    Id index_id = node.get_children().at(1);
    bool is_geqrf = false;
    for (const auto *enode : egraph.get_class_nodes(child_id)) {
        Atom child_atom = enode->get_atom();
        if (auto child_op = std::get_if<Op>(&child_atom)) {
            if (*child_op == Op::Geqrf) {
                is_geqrf = true;
                break;
            }
        }
    }
    if (is_geqrf) {
        for (const auto *index_enode : egraph.get_class_nodes(index_id)) {
            Atom index_atom = index_enode->get_atom();
            if (auto val = std::get_if<int>(&index_atom)) {
                if (*val == 0) {
                    return compute_orgqr_cost(Op::Orgqr, node, egraph, size_bindings) * 1.2;
                }
            }
        }
    }
    return 0.0;
}

Cost compute_scale_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        double rows = std::get<int>(shape.first);
        double cols = std::get<int>(shape.second);
        return rows * cols;
    } else {
        std::string r = size_to_symbol(shape.first);
        std::string c = size_to_symbol(shape.second);
        Monomial rc = {{r, c}};
        SymbolicCost sc;
        sc[rc] = 1.0;
        return sc;
    }
}

Cost compute_gemm_nn_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapeA = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    auto shapeB = get_one_shape(egraph, size_bindings, node.get_children().at(1));
    Size M, K, N;
    if (op == Gemm_NN) {
        M = shapeA.first;
        K = shapeA.second;
        N = shapeB.second;
    } else if (op == Gemm_TN) {
        M = shapeA.second;
        K = shapeA.first;
        N = shapeB.second;
    } else if (op == Gemm_NT) {
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

Cost compute_syrk_n_syrk_t_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    Size N_dim = (op == Syrk_N) ? shape.first : shape.second;
    Size K_dim = (op == Syrk_N) ? shape.second : shape.first;

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

Cost compute_trmm_ln_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapeB = get_one_shape(egraph, size_bindings, node.get_children().at(1));

    // Check if C is zero
    auto c_prop = egraph.get_class_analysis_data(node.get_children().at(2));
    bool is_c_zero = false;
    if (auto p = std::get_if<MatrixProperty>(&c_prop.property)) {
        is_c_zero = p->flags.is_zero;
    }

    Size M_dim, N_dim;
    bool is_left_solve = (op == Trmm_LN || op == Trmm_LT);

    M_dim = shapeB.first;
    N_dim = shapeB.second;

    if (std::holds_alternative<int>(M_dim) && std::holds_alternative<int>(N_dim)) {
        double M = std::get<int>(M_dim);
        double N = std::get<int>(N_dim);
        double cost = is_left_solve ? (M * M * N) : (M * N * N);
        if (!is_c_zero) {
            cost += M * N; // cost of daxpy
        }
        return Cost(cost);
    }

    SymbolicCost sc;
    if (is_left_solve) {
        Monomial m = {{size_to_symbol(M_dim), size_to_symbol(M_dim), size_to_symbol(N_dim)}};
        sc[m] = 1.0;
    } else {
        Monomial m = {{size_to_symbol(M_dim), size_to_symbol(N_dim), size_to_symbol(N_dim)}};
        sc[m] = 1.0;
    }

    if (!is_c_zero) {
        Monomial m_add = {{size_to_symbol(M_dim), size_to_symbol(N_dim)}};
        sc[m_add] += 1.0; // cost of daxpy
    }
    return sc;
}

Cost compute_symm_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapeB = get_one_shape(egraph, size_bindings, node.get_children().at(1));
    Size M, N;
    M = shapeB.first;
    N = shapeB.second;

    if (op == Symm_L) {
        if (std::holds_alternative<int>(M) && std::holds_alternative<int>(N)) {
            // C = A*B with A symmetric (M x M), B (M x N): ~M^2*N flops (half of gemm)
            return Cost(1.0 * std::get<int>(M) * std::get<int>(M) * std::get<int>(N));
        }
        Monomial m = {{size_to_symbol(M), size_to_symbol(M), size_to_symbol(N)}};
        SymbolicCost sc;
        sc[m] = 1.0;
        return sc;
    } else {
        if (std::holds_alternative<int>(M) && std::holds_alternative<int>(N)) {
            // C = B*A with A symmetric (N x N), B (M x N): ~M*N^2 flops (half of gemm)
            return Cost(1.0 * std::get<int>(M) * std::get<int>(N) * std::get<int>(N));
        }
        Monomial m = {{size_to_symbol(M), size_to_symbol(N), size_to_symbol(N)}};
        SymbolicCost sc;
        sc[m] = 1.0;
        return sc;
    }
}

Cost compute_trsm_ln_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapeA = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    auto shapeB = get_one_shape(egraph, size_bindings, node.get_children().at(1));
    Size M_dim, N_dim;

    bool is_left_solve = (op == Trsm_LN || op == Trsm_LT);

    if (is_left_solve) {
        M_dim = (op == Trsm_LN) ? shapeA.first : shapeA.second;
        N_dim = shapeB.second;
    } else {
        M_dim = shapeB.first;
        N_dim = (op == Trsm_RN) ? shapeA.second : shapeA.first;
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

Cost compute_potrf_l_potrf_u_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
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

Cost compute_geqrf_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
    if (is_numeric(shape)) {
        double rows = std::get<int>(shape.first);
        double cols = std::get<int>(shape.second);
        auto min_dim = std::min(rows, cols);
        auto max_dim = std::max(rows, cols);
        return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
    } else {
        std::string r = size_to_symbol(shape.first);
        std::string c = size_to_symbol(shape.second);
        if (auto data = get_matrix_data(egraph, node.get_children().at(0))) {
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

Cost compute_trtri_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shape = get_one_shape(egraph, size_bindings, node.get_children().at(0));
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

Cost compute_gemv_n_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapes = get_two_shapes(egraph, size_bindings, node.get_children().at(0), node.get_children().at(1));
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

Cost compute_gemv_t_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    auto shapes = get_two_shapes(egraph, size_bindings, node.get_children().at(0), node.get_children().at(1));
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

Cost compute_orgqr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    Id geqrf_id = node.get_children().at(0);
    auto tuple_data = egraph.get_class_analysis_data(geqrf_id);
    Shape shape = {{}, {}};
    if (auto *props = std::get_if<TupleProperty>(&tuple_data.property)) {
        if (!props->empty())
            shape = bind_shape((*props)[0].shape, size_bindings);
    }
    if (is_numeric(shape)) {
        double rows = std::get<int>(shape.first);
        double cols = std::get<int>(shape.second);
        auto min_dim = std::min(rows, cols);
        auto max_dim = std::max(rows, cols);
        return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
    } else {
        std::string r = size_to_symbol(shape.first);
        std::string c = size_to_symbol(shape.second);
        if (auto props = std::get_if<TupleProperty>(&tuple_data.property)) {
            if ((*props)[0].is_wide_matrix()) {
                std::swap(r, c);
            }
        }
        std::vector<std::string> mn2_vec{r, c, c};

        std::vector<std::string> n3_vec{c, c, c};

        SymbolicCost sc;
        sc[Monomial(mn2_vec)] = 2.0;
        sc[Monomial(n3_vec)] = -2.0 / 3.0;
        return sc;
    }
}

Cost compute_ormqr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    Id geqrf_id = node.get_children().at(0);
    auto tuple_data = egraph.get_class_analysis_data(geqrf_id);
    Shape shapeQ = {{}, {}};
    if (auto *props = std::get_if<TupleProperty>(&tuple_data.property)) {
        if (!props->empty())
            shapeQ = bind_shape((*props)[0].shape, size_bindings);
    }
    auto shapeC = get_one_shape(egraph, size_bindings, node.get_children().at(1));
    bool is_left = (op == Op::Ormqr_LN || op == Op::Ormqr_LT);
    if (is_numeric(shapeQ) && is_numeric(shapeC)) {
        double m = std::get<int>(shapeC.first);
        double n = std::get<int>(shapeC.second);
        double rowsA = std::get<int>(shapeQ.first);
        double colsA = std::get<int>(shapeQ.second);
        double k = std::min(rowsA, colsA);
        if (is_left) {
            return 4.0 * m * n * k - 2.0 * n * k * k + 3.0 * n * k;
        } else {
            return 4.0 * m * n * k - 2.0 * m * k * k + 3.0 * m * k;
        }
    } else {
        std::string m = size_to_symbol(shapeC.first);
        std::string n = size_to_symbol(shapeC.second);
        std::string k = size_to_symbol(shapeQ.second);
        if (auto props = std::get_if<TupleProperty>(&tuple_data.property)) {
            if (!props->empty() && (*props)[0].is_wide_matrix()) {
                k = size_to_symbol(shapeQ.first);
            }
        }

        std::vector<std::string> mnk_vec{m, n, k};
        SymbolicCost sc;
        sc[Monomial(mnk_vec)] = 4.0;
        if (is_left) {
            std::vector<std::string> nkk_vec{n, k, k};
            std::vector<std::string> nk_vec{n, k};
            sc[Monomial(nkk_vec)] = -2.0;
            sc[Monomial(nk_vec)] = 3.0;
        } else {
            std::vector<std::string> mkk_vec{m, k, k};
            std::vector<std::string> mk_vec{m, k};
            sc[Monomial(mkk_vec)] = -2.0;
            sc[Monomial(mk_vec)] = 3.0;
        }
        return sc;
    }
}

Cost compute_axpy_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings) {
    return compute_add_cost(op, node, egraph, size_bindings);
}

} // namespace egraph
