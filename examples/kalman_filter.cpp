#include "MatrixIO.h"
#include "api.h"
#include "examples.h"
#include "utils.h"
#include <iostream>
#include <utility>

using namespace egraph;

int run_kalman_filter() {
    auto [P_n_n_1_sizes, P_n_n_1_data] = read_matrix("data/kalman_p.csv");
    auto [H_sizes, H_data] = read_matrix("data/kalman_h.csv");
    auto [R_n_sizes, R_n_data] = read_matrix("data/kalman_r.csv");
    auto [x_hat_n_n_1_sizes, x_hat_n_n_1_data] = read_matrix("data/kalman_x.csv");
    auto [z_n_sizes, z_n_data] = read_matrix("data/kalman_z.csv");

    if (P_n_n_1_sizes.first <= 0 || P_n_n_1_sizes.first != P_n_n_1_sizes.second) {
        std::cerr << "Size error: P_{n, n-1} must be non-empty and square.\n";
        return 1;
    }
    if (R_n_sizes.first <= 0 || R_n_sizes.first != R_n_sizes.second) {
        std::cerr << "Size error: R_n must be non-empty and square.\n";
        return 1;
    }
    if (H_sizes.first >= H_sizes.second) {
        std::cerr << "Size error: H must be wide (rows < cols, measurement_dim < state_dim).\n";
        return 1;
    }
    if (H_sizes.second != P_n_n_1_sizes.first) {
        std::cerr << "Size error: Columns of H must match state dimension (rows of P_{n, n-1}).\n";
        return 1;
    }
    if (H_sizes.first != R_n_sizes.first) {
        std::cerr << "Size error: Rows of H must match measurement dimension (rows of R_n).\n";
        return 1;
    }
    if (x_hat_n_n_1_sizes.second != 1 || x_hat_n_n_1_sizes.first != P_n_n_1_sizes.first) {
        std::cerr << "Size error: x_hat_{n, n-1} must be a column vector matching state dimension.\n";
        return 1;
    }
    if (z_n_sizes.second != 1 || z_n_sizes.first != R_n_sizes.first) {
        std::cerr << "Size error: z_n must be a column vector matching measurement dimension.\n";
        return 1;
    }

    int m_val = P_n_n_1_sizes.first; // state dimension
    int p_val = R_n_sizes.first;     // measurement dimension

    SizeBindings concrete_sizes = {{"m", m_val}, {"p", p_val}};

    //    K_n = P_{n, n-1} * H^T * (H * P_{n, n-1} * H^T + R_n)^-1
    //    \hat{x}_{n, n} = \hat{x}_{n, n-1} + K_n * (z_n - H * \hat{x}_{n, n-1})
    EGraphRunner::Context ctx1;
    ctx1.get_config().enable_logging = true;

    Expression P_n_n_1 = ctx1.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H = ctx1.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n = ctx1.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});
    Expression x_hat_n_n_1 = ctx1.define_matrix("x_hat_n_n_1", "m", 1);
    Expression z_n = ctx1.define_matrix("z_n", "p", 1);

    Expression K_n = P_n_n_1 * transpose(H) * inverse(H * P_n_n_1 * transpose(H) + R_n);
    Expression x_hat_n_n = x_hat_n_n_1 + K_n * (z_n - H * x_hat_n_n_1);

    ctx1.optimize_symbolic(x_hat_n_n, {K_n});

    DataBindings state_data = {
        {"P_n_n_1", P_n_n_1_data},
        {"H", H_data},
        {"R_n", R_n_data},
        {"x_hat_n_n_1", x_hat_n_n_1_data},
        {"z_n", z_n_data}};

    auto out_x_hat_n_n = ctx1.evaluate_concrete(concrete_sizes, state_data);
    auto out_K_n = ctx1.get_preserved(K_n);

    write_matrix("data/kalman_result_K_n.csv", m_val, p_val, out_K_n);
    write_matrix("data/kalman_result_x_hat_n_n.csv", m_val, 1, out_x_hat_n_n);

    //    P_{n, n} = (I - K_n * H) * P_{n, n-1} * (I - K_n * H)^T + K_n * R_n * K_n^T
    EGraphRunner::Context ctx2;
    ctx2.get_config().enable_logging = false;

    Expression P_n_n_1_v2 = ctx2.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H_v2 = ctx2.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n_v2 = ctx2.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});
    Expression I = ctx2.define_matrix("I", "m", "m", {"identity"});

    Expression K_n_v2 = P_n_n_1_v2 * transpose(H_v2) * inverse(H_v2 * P_n_n_1_v2 * transpose(H_v2) + R_n_v2);
    Expression I_K_n_H = I - K_n_v2 * H_v2;
    Expression P_n_n = I_K_n_H * P_n_n_1_v2 * transpose(I_K_n_H) + K_n_v2 * R_n_v2 * transpose(K_n_v2);

    ctx2.optimize_symbolic(P_n_n, {K_n_v2});

    DataBindings cov_data = {
        {"P_n_n_1", P_n_n_1_data}, {"H", H_data}, {"R_n", R_n_data}, {"I", generate_identity_matrix(m_val)}};

    auto out_P_n_n = ctx2.evaluate_concrete(concrete_sizes, cov_data);

    write_matrix("data/kalman_result_P_n_n.csv", m_val, m_val, out_P_n_n);

    return 0;
}
