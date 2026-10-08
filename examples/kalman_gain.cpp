#include "MatrixIO.h"
#include "api.h"
#include "examples.h"
#include <iostream>
#include <utility>

using namespace egraph;

int run_kalman_gain() {
    //    K_n = P_{n, n-1} * H^T * (H * P_{n, n-1} * H^T + R_n)^-1
    auto [P_n_n_1_sizes, P_n_n_1_data] = read_matrix("data/kalman_p.csv");
    auto [H_sizes, H_data] = read_matrix("data/kalman_h.csv");
    auto [R_n_sizes, R_n_data] = read_matrix("data/kalman_r.csv");

    // print sizes
    std::cout << "P_n_n_1_sizes: " << P_n_n_1_sizes.first << " x " << P_n_n_1_sizes.second << "\n";
    std::cout << "H_sizes: " << H_sizes.first << " x " << H_sizes.second << "\n";
    std::cout << "R_n_sizes: " << R_n_sizes.first << " x " << R_n_sizes.second << "\n";

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

    int m_val = P_n_n_1_sizes.first; // state dimension
    int p_val = R_n_sizes.first;     // measurement dimension

    SizeBindings concrete_sizes = {{"m", m_val}, {"p", p_val}};

    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = true;

    Expression P_n_n_1 = ctx.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H = ctx.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n = ctx.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});

    Expression S_n = H * P_n_n_1 * transpose(H) + R_n;
    Expression K_n = P_n_n_1 * transpose(H) * inverse(S_n);

    ctx.optimize_symbolic(K_n);

    DataBindings concrete_data = {{"P_n_n_1", P_n_n_1_data}, {"H", H_data}, {"R_n", R_n_data}};

    auto out_K_n = ctx.evaluate_concrete(concrete_sizes, concrete_data);

    write_matrix("data/kalman_result_K_n.csv", m_val, p_val, out_K_n);

    std::cout << "Kalman Gain calculation completed successfully.\n";
    std::cout << "State dimension: " << m_val << ", Measurement dimension: " << p_val << "\n";
    std::cout << "Calculated K_n (" << m_val << "x" << p_val << ") -> data/kalman_result_K_n.csv\n";
    std::cout << "Extraction: " << ctx.get_last_extraction_duration_ms()
              << " ms, evaluation: " << ctx.get_last_evaluation_duration_ms() << " ms\n";

    return 0;
}
