#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
using namespace egraph;

using namespace EGraphRunner;

constexpr double kMaxOlsTimeMs = 20.0;
constexpr double kMaxGlsTimeMs = 250.0;
constexpr double kMaxStoTimeMs = 100.0; // current pruning strategy fails to find optimal solution but it does exist
constexpr double kMaxImageRestorationTimeMs = 400.0;
constexpr double kMaxKalmanGainTimeMs = 150.0;
constexpr double kMaxKalmanTimeMs = 300.0;

TEST(SystemTest, OLS) {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;
    Expression X = ctx.define_matrix("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix("y", "a", 1);
    Expression target_math = (inverse(transpose(X) * X) * transpose(X)) * y;

    ctx.optimize_symbolic(target_math);
    auto [x_sizes, x_data] = read_matrix("data/ols_x.csv");
    auto [y_sizes, y_data] = read_matrix("data/ols_y.csv");

    if (x_sizes.first <= 0 || x_sizes.second <= 0) {
        std::cerr << "Size error: X must be non-empty.\n";
    }
    if (y_sizes.second != 1) {
        std::cerr << "Size error: y must be a column vector.\n";
    }
    if (x_sizes.first != y_sizes.first) {
        std::cerr << "Size error: X and y must have the same number of rows.\n";
    }
    if (x_sizes.first <= x_sizes.second) {
        std::cerr << "Size error: X must be tall (rows > cols).\n";
    }

    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"y", y_data}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "data/ols_result.csv";
    write_matrix(out_path, row, col, out);

    double total_ms = ctx.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxOlsTimeMs) << "OLS time exceeded benchmark threshold. Extraction: "
                                       << ctx.get_last_extraction_duration_ms()
                                       << " ms, Evaluation: " << ctx.get_last_evaluation_duration_ms()
                                       << " ms, Total: " << total_ms << " ms";
}

TEST(SystemTest, GLS) {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;
    Expression X = ctx.define_matrix("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix("y", "a", 1);
    Expression M = ctx.define_matrix("M", "a", "a", {"symmetric", "positive_definite"});
    Expression target_math = inverse(transpose(X) * inverse(M) * X) * transpose(X) * inverse(M) * y;
    ctx.optimize_symbolic(target_math);
    auto [m_sizes, m_data] = read_matrix("data/gls_m.csv");
    auto [x_sizes, x_data] = read_matrix("data/gls_x.csv");
    auto [y_sizes, y_data] = read_matrix("data/gls_y.csv");

    if (m_sizes.first != m_sizes.second) {
        std::cerr << "Size error: M must be square.\n";
    }
    if (y_sizes.second != 1) {
        std::cerr << "Size error: y must be column vectors.\n";
    }
    if (x_sizes.first != y_sizes.first) {
        std::cerr << "Size error: X and y must have the same number of rows.\n";
    }
    if (m_sizes.first != x_sizes.first) {
        std::cerr << "Size error: M and X must have the same number of rows.\n";
    }

    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"M", m_data}, {"y", y_data}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "data/gls_result.csv";
    write_matrix(out_path, row, col, out);

    double total_ms = ctx.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxGlsTimeMs) << "GLS time exceeded benchmark threshold. Extraction: "
                                       << ctx.get_last_extraction_duration_ms()
                                       << " ms, Evaluation: " << ctx.get_last_evaluation_duration_ms()
                                       << " ms, Total: " << total_ms << " ms";
}

TEST(SystemTest, StochasticNewton) {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;

    // Symbolic Matrix Definitions (generic sizes: b, a, l)
    Expression B = ctx.define_matrix("B", "b", "b", {"positive_definite", "symmetric"});
    Expression In = ctx.define_matrix("In", "b", "b", {"identity"});
    Expression A = ctx.define_matrix("A", "a", "b", {"full_rank", "tall"});
    Expression W_k = ctx.define_matrix("W_k", "a", "l", {"full_rank", "tall"});
    Expression Il = ctx.define_matrix("Il", "l", "l", {"identity"});

    // Formula: B_k = (k / (k - 1)) * B * (In - transpose(A) * W_k * inverse((k - 1) * Il + transpose(W_k) * A * B *
    // transpose(A) * W_k) * transpose(W_k) * A * B)
    ScalarExpr k('k');
    ScalarExpr scale_factor = k / (k - 1.0);

    Expression inner_inv = inverse(scale(Il, k - 1.0) + transpose(W_k) * A * B * transpose(A) * W_k);
    Expression inner_term = In - transpose(A) * W_k * inner_inv * transpose(W_k) * A * B;
    Expression target_math = scale(B * inner_term, scale_factor);

    ctx.optimize_symbolic(target_math);

    // Read concrete matrix files
    auto [b_sizes, b_data] = read_matrix("data/stochastic_newton_b.csv");
    auto [in_sizes, in_data] = read_matrix("data/stochastic_newton_in.csv");
    auto [a_sizes, a_data] = read_matrix("data/stochastic_newton_a.csv");
    auto [w_sizes, w_data] = read_matrix("data/stochastic_newton_w.csv");
    auto [il_sizes, il_data] = read_matrix("data/stochastic_newton_il.csv");

    if (b_sizes.first <= 0 || b_sizes.second <= 0 || b_sizes.first != b_sizes.second) {
        std::cerr << "Size error: B must be square.\n";
    }
    if (in_sizes != b_sizes) {
        std::cerr << "Size error: In must match dimensions of B.\n";
    }
    if (a_sizes.second != b_sizes.first) {
        std::cerr << "Size error: Columns of A must match rows of B.\n";
    }
    if (w_sizes.first != a_sizes.first) {
        std::cerr << "Size error: Rows of W_k must match rows of A.\n";
    }
    if (il_sizes.first != w_sizes.second || il_sizes.second != w_sizes.second) {
        std::cerr << "Size error: Il must be square with size equal to columns of W_k.\n";
    }

    // Concrete Size & Data Bindings (given evaluation sizes: b=1000, a=5000, l=625, k=10)
    SizeBindings concrete_sizes = {{"b", b_sizes.second}, {"a", a_sizes.first}, {"l", w_sizes.second}};
    DataBindings concrete_data = {{"B", b_data},   {"In", in_data}, {"A", a_data},
                                  {"W_k", w_data}, {"Il", il_data}, {"k", std::vector<double>{10.0}}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "data/stochastic_newton_result.csv";
    write_matrix(out_path, row, col, out);

    double total_ms = ctx.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxStoTimeMs) << "Stochastic Newton time exceeded benchmark threshold. Extraction: "
                                       << ctx.get_last_extraction_duration_ms()
                                       << " ms, Evaluation: " << ctx.get_last_evaluation_duration_ms()
                                       << " ms, Total: " << total_ms << " ms";
}

TEST(SystemTest, ImageRestoration) {
    auto [h_sizes, h_data] = read_matrix("data/image_h.csv");
    auto [y_sizes, y_data] = read_matrix("data/image_y.csv");
    auto [x_sizes, x_data] = read_matrix("data/image_x.csv");

    if (h_sizes.first >= h_sizes.second) {
        std::cerr << "Size error: H must be wide (rows < cols).\n";
    }
    if (y_sizes.second != 1 || x_sizes.second != 1) {
        std::cerr << "Size error: y and x must be column vectors.\n";
    }
    if (h_sizes.first != y_sizes.first) {
        std::cerr << "Size error: H and y must have the same number of rows.\n";
    }
    if (h_sizes.second != x_sizes.first) {
        std::cerr << "Size error: H and x must agree on the inner dimension.\n";
    }

    SizeBindings concrete_sizes = {{"a", h_sizes.first}, {"b", h_sizes.second}};

    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;

    Expression y = ctx.define_matrix("y", "a", 1);
    Expression H = ctx.define_matrix("H", "a", "b", {"full_rank", "wide"});
    Expression I = ctx.define_matrix("I", "b", "b", {"identity"});
    Expression x = ctx.define_matrix("x", "b", 1);

    Expression math_1 = transpose(H) * inverse(H * transpose(H));
    Expression math_2 = math_1 * y + (I - math_1 * H) * x;

    // math_1 passed as a background expression
    ctx.optimize_symbolic(math_2, {math_1});

    DataBindings concrete_data = {
        {"y", y_data}, {"H", h_data}, {"I", generate_identity_matrix(h_sizes.second)}, {"x", x_data}};

    // run extraction for both math_1 and math_2 if math_1 is not computed explicitly when computing math_2 (does not
    // appear as a e-class in the extraction result of math_2)
    auto out2 = ctx.evaluate_concrete(concrete_sizes, concrete_data);

    auto out1 = ctx.get_preserved(math_1);
    auto out1_shape = bind_shape(ctx.get_property(math_1).shape, &concrete_sizes);
    int row1 = std::get<int>(out1_shape.first);
    int col1 = std::get<int>(out1_shape.second);
    std::string out_path1 = "data/image_result_1.csv";
    write_matrix(out_path1, row1, col1, out1);

    // Retrieve target math_2
    auto out2_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row2 = std::get<int>(out2_shape.first);
    int col2 = std::get<int>(out2_shape.second);
    std::string out_path2 = "data/image_result_2.csv";
    write_matrix(out_path2, row2, col2, out2);

    double total_ms = ctx.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxImageRestorationTimeMs)
        << "Image Restoration time exceeded benchmark threshold. Extraction: " << ctx.get_last_extraction_duration_ms()
        << " ms, Evaluation: " << ctx.get_last_evaluation_duration_ms() << " ms, Total: " << total_ms << " ms";
}

TEST(SystemTest, KalmanGain) {
    auto [P_n_n_1_sizes, P_n_n_1_data] = read_matrix("data/kalman_P_n_n_1.csv");
    if (P_n_n_1_data.empty()) {
        std::tie(P_n_n_1_sizes, P_n_n_1_data) = read_matrix("data/kalman_p.csv");
    }
    auto [H_sizes, H_data] = read_matrix("data/kalman_H.csv");
    if (H_data.empty()) {
        std::tie(H_sizes, H_data) = read_matrix("data/kalman_h.csv");
    }
    auto [R_n_sizes, R_n_data] = read_matrix("data/kalman_R_n.csv");
    if (R_n_data.empty()) {
        std::tie(R_n_sizes, R_n_data) = read_matrix("data/kalman_r.csv");
    }

    if (P_n_n_1_sizes.first <= 0 || P_n_n_1_sizes.first != P_n_n_1_sizes.second) {
        std::cerr << "Size error: P_{n, n-1} must be non-empty and square.\n";
    }
    if (R_n_sizes.first <= 0 || R_n_sizes.first != R_n_sizes.second) {
        std::cerr << "Size error: R_n must be non-empty and square.\n";
    }
    if (H_sizes.first >= H_sizes.second) {
        std::cerr << "Size error: H must be wide (rows < cols).\n";
    }
    if (H_sizes.second != P_n_n_1_sizes.first) {
        std::cerr << "Size error: Columns of H must match state dimension (rows of P_{n, n-1}).\n";
    }
    if (H_sizes.first != R_n_sizes.first) {
        std::cerr << "Size error: Rows of H must match measurement dimension (rows of R_n).\n";
    }

    int m_val = P_n_n_1_sizes.first; // state dimension
    int p_val = R_n_sizes.first;     // measurement dimension
    SizeBindings concrete_sizes = {{"m", m_val}, {"p", p_val}};

    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;

    Expression P_n_n_1 = ctx.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H = ctx.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n = ctx.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});

    // 1. Compute Kalman Gain:
    //    K_n = P_{n, n-1} * H^T * (H * P_{n, n-1} * H^T + R_n)^-1
    Expression S_n = H * P_n_n_1 * transpose(H) + R_n;
    Expression K_n = P_n_n_1 * transpose(H) * inverse(S_n);

    ctx.optimize_symbolic(K_n);

    DataBindings concrete_data = {
        {"P_n_n_1", P_n_n_1_data},
        {"H", H_data},
        {"R_n", R_n_data}
    };

    auto out_K_n = ctx.evaluate_concrete(concrete_sizes, concrete_data);

    write_matrix("data/kalman_result_K_n.csv", m_val, p_val, out_K_n);

    double total_ms = ctx.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxKalmanGainTimeMs)
        << "Kalman Gain time exceeded benchmark threshold. Extraction: " << ctx.get_last_extraction_duration_ms()
        << " ms, Evaluation: " << ctx.get_last_evaluation_duration_ms() << " ms, Total: " << total_ms << " ms";
}

TEST(SystemTest, KalmanFilter) {
    auto [P_n_n_1_sizes, P_n_n_1_data] = read_matrix("data/kalman_P_n_n_1.csv");
    if (P_n_n_1_data.empty()) {
        std::tie(P_n_n_1_sizes, P_n_n_1_data) = read_matrix("data/kalman_p.csv");
    }
    auto [H_sizes, H_data] = read_matrix("data/kalman_H.csv");
    if (H_data.empty()) {
        std::tie(H_sizes, H_data) = read_matrix("data/kalman_h.csv");
    }
    auto [R_n_sizes, R_n_data] = read_matrix("data/kalman_R_n.csv");
    if (R_n_data.empty()) {
        std::tie(R_n_sizes, R_n_data) = read_matrix("data/kalman_r.csv");
    }
    auto [x_hat_n_n_1_sizes, x_hat_n_n_1_data] = read_matrix("data/kalman_x_hat_n_n_1.csv");
    if (x_hat_n_n_1_data.empty()) {
        std::tie(x_hat_n_n_1_sizes, x_hat_n_n_1_data) = read_matrix("data/kalman_x.csv");
    }
    auto [z_n_sizes, z_n_data] = read_matrix("data/kalman_z_n.csv");
    if (z_n_data.empty()) {
        std::tie(z_n_sizes, z_n_data) = read_matrix("data/kalman_z.csv");
    }

    if (P_n_n_1_sizes.first <= 0 || P_n_n_1_sizes.first != P_n_n_1_sizes.second) {
        std::cerr << "Size error: P_{n, n-1} must be non-empty and square.\n";
    }
    if (R_n_sizes.first <= 0 || R_n_sizes.first != R_n_sizes.second) {
        std::cerr << "Size error: R_n must be non-empty and square.\n";
    }
    if (H_sizes.first >= H_sizes.second) {
        std::cerr << "Size error: H must be wide (rows < cols).\n";
    }
    if (H_sizes.second != P_n_n_1_sizes.first) {
        std::cerr << "Size error: Columns of H must match state dimension (rows of P_{n, n-1}).\n";
    }
    if (H_sizes.first != R_n_sizes.first) {
        std::cerr << "Size error: Rows of H must match measurement dimension (rows of R_n).\n";
    }
    if (x_hat_n_n_1_sizes.second != 1 || x_hat_n_n_1_sizes.first != P_n_n_1_sizes.first) {
        std::cerr << "Size error: x_hat_{n, n-1} must be a column vector matching state dimension.\n";
    }
    if (z_n_sizes.second != 1 || z_n_sizes.first != R_n_sizes.first) {
        std::cerr << "Size error: z_n must be a column vector matching measurement dimension.\n";
    }

    int m_val = P_n_n_1_sizes.first; // state dimension
    int p_val = R_n_sizes.first;     // measurement dimension
    SizeBindings concrete_sizes = {{"m", m_val}, {"p", p_val}};

    // --- State Update ---
    // 1. Compute Kalman Gain:
    //    K_n = P_{n, n-1} * H^T * (H * P_{n, n-1} * H^T + R_n)^-1
    // 2. Update estimate with measurement:
    //    \hat{x}_{n, n} = \hat{x}_{n, n-1} + K_n * (z_n - H * \hat{x}_{n, n-1})
    EGraphRunner::Context ctx_state;
    ctx_state.get_config().enable_logging = false;

    Expression P_n_n_1 = ctx_state.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H = ctx_state.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n = ctx_state.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});
    Expression x_hat_n_n_1 = ctx_state.define_matrix("x_hat_n_n_1", "m", 1);
    Expression z_n = ctx_state.define_matrix("z_n", "p", 1);

    Expression S_n = H * P_n_n_1 * transpose(H) + R_n;
    Expression K_n = P_n_n_1 * transpose(H) * inverse(S_n);
    Expression x_hat_n_n = x_hat_n_n_1 + K_n * (z_n - H * x_hat_n_n_1);

    ctx_state.optimize_symbolic(x_hat_n_n, {K_n});

    DataBindings state_data = {
        {"P_n_n_1", P_n_n_1_data},
        {"H", H_data},
        {"R_n", R_n_data},
        {"x_hat_n_n_1", x_hat_n_n_1_data},
        {"z_n", z_n_data}
    };

    auto out_x_hat_n_n = ctx_state.evaluate_concrete(concrete_sizes, state_data);
    auto out_K_n = ctx_state.get_preserved(K_n);

    // Save exact matrices 1 & 2: K_n and \hat{x}_{n, n}
    write_matrix("data/kalman_result_K_n.csv", m_val, p_val, out_K_n);
    write_matrix("data/kalman_result_x_hat_n_n.csv", m_val, 1, out_x_hat_n_n);

    // --- Covariance Update (Joseph stabilized form) ---
    // 3. Update the estimate uncertainty:
    //    P_{n, n} = (I - K_n * H) * P_{n, n-1} * (I - K_n * H)^T + K_n * R_n * K_n^T
    EGraphRunner::Context ctx_cov;
    ctx_cov.get_config().enable_logging = false;

    Expression P_n_n_1_cov = ctx_cov.define_matrix("P_n_n_1", "m", "m", {"symmetric", "positive_definite"});
    Expression H_cov = ctx_cov.define_matrix("H", "p", "m", {"full_rank", "wide"});
    Expression R_n_cov = ctx_cov.define_matrix("R_n", "p", "p", {"symmetric", "positive_definite"});
    Expression I = ctx_cov.define_matrix("I", "m", "m", {"identity"});

    Expression S_n_cov = H_cov * P_n_n_1_cov * transpose(H_cov) + R_n_cov;
    Expression K_n_cov = P_n_n_1_cov * transpose(H_cov) * inverse(S_n_cov);
    Expression I_K_n_H = I - K_n_cov * H_cov;
    Expression P_n_n = I_K_n_H * P_n_n_1_cov * transpose(I_K_n_H) + K_n_cov * R_n_cov * transpose(K_n_cov);

    ctx_cov.optimize_symbolic(P_n_n, {K_n_cov});

    DataBindings cov_data = {
        {"P_n_n_1", P_n_n_1_data},
        {"H", H_data},
        {"R_n", R_n_data},
        {"I", generate_identity_matrix(m_val)}
    };

    auto out_P_n_n = ctx_cov.evaluate_concrete(concrete_sizes, cov_data);

    // Save exact matrix 3: P_{n, n}
    write_matrix("data/kalman_result_P_n_n.csv", m_val, m_val, out_P_n_n);

    double total_ms = ctx_state.get_last_total_duration_ms() + ctx_cov.get_last_total_duration_ms();
    EXPECT_LT(total_ms, kMaxKalmanTimeMs)
        << "Kalman Filter time exceeded benchmark threshold. State: " << ctx_state.get_last_total_duration_ms()
        << " ms, Covariance: " << ctx_cov.get_last_total_duration_ms() << " ms, Total: " << total_ms << " ms";
}