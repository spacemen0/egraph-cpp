#include "op_costs.h"
#include <algorithm>

namespace egraph {

Cost compute_add_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return static_cast<double>(mat0->rows * mat0->cols);
}

Cost compute_mul_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    bool is_sq0 = mat0->is_square();
    bool is_sq1 = mat1->is_square();

    bool is_trmm_left = is_sq0 && (mat0->is_upper_triangular() || mat0->is_lower_triangular());
    bool is_trmm_right = is_sq1 && (mat1->is_upper_triangular() || mat1->is_lower_triangular());

    double coeff = (is_trmm_left || is_trmm_right) ? 1.0 : 2.0;
    return coeff * mat0->rows * mat0->cols * mat1->cols;
}

Cost compute_tr_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return static_cast<double>(mat0->rows * mat0->cols);
}

Cost compute_inv_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    int rows = mat0->rows;
    if (mat0->is_upper_triangular() || mat0->is_lower_triangular() || mat0->is_diagonal()) {
        return (1.0 / 3.0) * rows * rows * rows;
    }
    return 24.0 * rows * rows * rows;
}

Cost compute_minus_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return static_cast<double>(mat0->rows * mat0->cols);
}

Cost compute_qr_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    double cols = mat0->cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_lu_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    return (2.0 / 3.0) * rows * rows * rows;
}

Cost compute_cholel_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_choleu_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_get_cost(Op op, const std::vector<Operand> &operands) { return 0.0; }

Cost compute_scale_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return static_cast<double>(mat0->rows) * mat0->cols;
}

Cost compute_gemm_nn_group_cost(Op op, const std::vector<Operand> &operands) {
    int M, K, N;
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    auto op_name = OpRegistry::name(op);
    if (op_name == "Gemm_NN") {
        M = mat0->rows;
        K = mat0->cols;
        N = mat1->cols;
    } else if (op_name == "Gemm_TN") {
        M = mat0->cols;
        K = mat0->rows;
        N = mat1->cols;
    } else if (op_name == "Gemm_NT") {
        M = mat0->rows;
        K = mat0->cols;
        N = mat1->rows;
    } else { // Gemm_TT
        M = mat0->cols;
        K = mat0->rows;
        N = mat1->rows;
    }
    return 2.0 * M * K * N;
}

Cost compute_syrk_n_syrk_t_cost(Op op, const std::vector<Operand> &operands) {
    auto op_name = OpRegistry::name(op);
    const auto *mat0 = as_matrix(operands[0]);
    int n_val = (op_name == "Syrk_N") ? mat0->rows : mat0->cols;
    int k_val = (op_name == "Syrk_N") ? mat0->cols : mat0->rows;
    return 1.0 * n_val * (n_val + 1.0) * k_val;
}

Cost compute_trmm_ln_group_cost(Op op, const std::vector<Operand> &operands) {
    auto op_name = OpRegistry::name(op);
    bool is_left_solve = (op_name == "Trmm_LN" || op_name == "Trmm_LT");

    const auto *mat1 = as_matrix(operands[1]);
    double M = mat1->rows;
    double N = mat1->cols;
    return is_left_solve ? (M * M * N) : (M * N * N);
}

Cost compute_symm_group_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat1 = as_matrix(operands[1]);
    double M = mat1->rows;
    double N = mat1->cols;
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
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);

    if (is_left_solve) {
        m_val = (op_name == "Trsm_LN") ? mat0->rows : mat0->cols;
        n_val = mat1->cols;
        return 1.0 * m_val * m_val * n_val; // M^2 * N
    } else {
        m_val = mat1->rows;
        n_val = (op_name == "Trsm_RN") ? mat0->cols : mat0->rows;
        return 1.0 * m_val * n_val * n_val; // M * N^2
    }
}

Cost compute_potrf_l_potrf_u_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_geqrf_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    double cols = mat0->cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_trtri_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    double rows = mat0->rows;
    return (1.0 / 3.0) * rows * rows * rows;
}

Cost compute_gemv_n_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return 2.0 * mat0->rows * mat0->cols;
}

Cost compute_gemv_t_cost(Op op, const std::vector<Operand> &operands) {
    const auto *mat0 = as_matrix(operands[0]);
    return 2.0 * mat0->rows * mat0->cols;
}

Cost compute_orgqr_cost(Op op, const std::vector<Operand> &operands) {
    const auto *tuple = as_tuple(operands[0]);
    const auto *mat0 = as_matrix(operands[0]);
    const MatrixOperand &target = (tuple && !tuple->empty()) ? (*tuple)[0] : *mat0;
    double rows = target.rows;
    double cols = target.cols;
    auto min_dim = std::min(rows, cols);
    auto max_dim = std::max(rows, cols);
    return 2.0 * min_dim * min_dim * max_dim - (2.0 / 3.0) * min_dim * min_dim * min_dim;
}

Cost compute_ormqr_cost(Op op, const std::vector<Operand> &operands) {
    auto op_name = OpRegistry::name(op);
    bool is_left = (op_name == "Ormqr_LN" || op_name == "Ormqr_LT");
    const auto *mat1 = as_matrix(operands[1]);
    double m = mat1->rows;
    double n = mat1->cols;
    const auto *tuple = as_tuple(operands[0]);
    const auto *mat0 = as_matrix(operands[0]);
    const MatrixOperand &target = (tuple && !tuple->empty()) ? (*tuple)[0] : *mat0;
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
