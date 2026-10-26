#include "op_costs.h"
#include <algorithm>

namespace egraph {

Cost compute_add_cost(Op op, const std::vector<Operand> &operands) {
    return static_cast<double>(operands[0].rows * operands[0].cols);
}

Cost compute_mul_cost(Op op, const std::vector<Operand> &operands) {
    bool is_sq0 = operands[0].is_square();
    bool is_sq1 = operands[1].is_square();

    bool is_trmm_left = is_sq0 && (operands[0].is_upper_triangular() || operands[0].is_lower_triangular());
    bool is_trmm_right = is_sq1 && (operands[1].is_upper_triangular() || operands[1].is_lower_triangular());

    double coeff = (is_trmm_left || is_trmm_right) ? 1.0 : 2.0;
    return coeff * operands[0].rows * operands[0].cols * operands[1].cols;
}

Cost compute_tr_cost(Op op, const std::vector<Operand> &operands) {
    return static_cast<double>(operands[0].rows * operands[0].cols);
}

Cost compute_inv_cost(Op op, const std::vector<Operand> &operands) {
    int rows = operands[0].rows;
    if (operands[0].is_upper_triangular() || operands[0].is_lower_triangular() || operands[0].is_diagonal()) {
        return (1.0 / 3.0) * rows * rows * rows;
    }
    return 24.0 * rows * rows * rows;
}

Cost compute_minus_cost(Op op, const std::vector<Operand> &operands) {
    return static_cast<double>(operands[0].rows * operands[0].cols);
}

Cost compute_qr_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    double cols = operands[0].cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_lu_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    return (2.0 / 3.0) * rows * rows * rows;
}

Cost compute_cholel_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_choleu_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_get_cost(Op op, const std::vector<Operand> &operands) { return 0.0; }

Cost compute_scale_cost(Op op, const std::vector<Operand> &operands) {
    return static_cast<double>(operands[0].rows) * operands[0].cols;
}

Cost compute_gemm_nn_group_cost(Op op, const std::vector<Operand> &operands) {
    int M, K, N;
    auto op_name = OpRegistry::name(op);
    if (op_name == "Gemm_NN") {
        M = operands[0].rows;
        K = operands[0].cols;
        N = operands[1].cols;
    } else if (op_name == "Gemm_TN") {
        M = operands[0].cols;
        K = operands[0].rows;
        N = operands[1].cols;
    } else if (op_name == "Gemm_NT") {
        M = operands[0].rows;
        K = operands[0].cols;
        N = operands[1].rows;
    } else { // Gemm_TT
        M = operands[0].cols;
        K = operands[0].rows;
        N = operands[1].rows;
    }
    return 2.0 * M * K * N;
}

Cost compute_syrk_n_syrk_t_cost(Op op, const std::vector<Operand> &operands) {
    auto op_name = OpRegistry::name(op);
    int n_val = (op_name == "Syrk_N") ? operands[0].rows : operands[0].cols;
    int k_val = (op_name == "Syrk_N") ? operands[0].cols : operands[0].rows;
    return 1.0 * n_val * (n_val + 1.0) * k_val;
}

Cost compute_trmm_ln_group_cost(Op op, const std::vector<Operand> &operands) {
    bool is_c_zero = operands.size() > 2 && operands[2].is_zero();
    auto op_name = OpRegistry::name(op);
    bool is_left_solve = (op_name == "Trmm_LN" || op_name == "Trmm_LT");

    double M = operands[1].rows;
    double N = operands[1].cols;
    double cost = is_left_solve ? (M * M * N) : (M * N * N);
    if (!is_c_zero) {
        cost += M * N; // cost of daxpy
    }
    return cost;
}

Cost compute_symm_group_cost(Op op, const std::vector<Operand> &operands) {
    double M = operands[1].rows;
    double N = operands[1].cols;
    auto op_name = OpRegistry::name(op);
    if (op_name == "Symm_L") {
        return 2.0 * M * M * N;
    } else {
        return 2.0 * M * N * N;
    }
}

Cost compute_trsm_ln_group_cost(Op op, const std::vector<Operand> &operands) {
    int m_val, n_val;
    auto op_name = OpRegistry::name(op);
    bool is_left_solve = (op_name == "Trsm_LN" || op_name == "Trsm_LT");

    if (is_left_solve) {
        m_val = (op_name == "Trsm_LN") ? operands[0].rows : operands[0].cols;
        n_val = operands[1].cols;
        return 1.0 * m_val * m_val * n_val; // M^2 * N
    } else {
        m_val = operands[1].rows;
        n_val = (op_name == "Trsm_RN") ? operands[0].cols : operands[0].rows;
        return 1.0 * m_val * n_val * n_val; // M * N^2
    }
}

Cost compute_potrf_l_potrf_u_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_geqrf_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    double cols = operands[0].cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_trtri_cost(Op op, const std::vector<Operand> &operands) {
    double rows = operands[0].rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_gemv_n_cost(Op op, const std::vector<Operand> &operands) {
    return 2.0 * operands[0].rows * operands[0].cols;
}

Cost compute_gemv_t_cost(Op op, const std::vector<Operand> &operands) {
    return 2.0 * operands[0].rows * operands[0].cols;
}

Cost compute_orgqr_cost(Op op, const std::vector<Operand> &operands) {
    const Operand &target = operands[0].is_tuple() ? operands[0].tuple[0] : operands[0];
    double rows = target.rows;
    double cols = target.cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_ormqr_cost(Op op, const std::vector<Operand> &operands) {
    auto op_name = OpRegistry::name(op);
    bool is_left = (op_name == "Ormqr_LN" || op_name == "Ormqr_LT");
    double m = operands[1].rows;
    double n = operands[1].cols;
    const Operand &target = operands[0].is_tuple() ? operands[0].tuple[0] : operands[0];
    double rowsA = target.rows;
    double colsA = target.cols;
    double k = std::min(rowsA, colsA);
    if (is_left) {
        return 4.0 * m * n * k - 2.0 * n * k * k + 3.0 * n * k;
    } else {
        return 4.0 * m * n * k - 2.0 * m * k * k + 3.0 * m * k;
    }
}

Cost compute_axpy_cost(Op op, const std::vector<Operand> &operands) { return compute_add_cost(op, operands); }

} // namespace egraph
