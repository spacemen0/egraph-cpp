#include "MatrixIO.h"
#include "api.h"
#include "examples.h"
#include "utils.h"
#include <iostream>
#include <utility>
using namespace egraph;

int run_stochastic_newton() {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = true;

    // Symbolic Matrix Definitions (generic sizes: b, a, l)
    Expression B = ctx.define_matrix_symbolic("B", "b", "b", {"positive_definite", "symmetric"});
    Expression In = ctx.define_matrix_symbolic("In", "b", "b", {"identity"});
    Expression A = ctx.define_matrix_symbolic("A", "a", "b", {"full_rank", "tall"});
    Expression W_k = ctx.define_matrix_symbolic("W_k", "a", "l", {"full_rank", "tall"});
    Expression Il = ctx.define_matrix_symbolic("Il", "l", "l", {"identity"});

    // Formula: B_k = (k / (k - 1)) * B * (In - transpose(A) * W_k * inverse((k - 1) * Il + transpose(W_k) * A * B *
    // transpose(A) * W_k) * transpose(W_k) * A * B)
    ScalarExpr k('k');
    ScalarExpr scale_factor = k / (k - 1.0);

    Expression inner_inv = inverse(scale(Il, k - 1.0) + transpose(W_k) * A * B * transpose(A) * W_k);
    Expression inner_term = In - transpose(A) * W_k * inner_inv * transpose(W_k) * A * B;
    Expression target_math = scale(B * inner_term, scale_factor);

    ctx.optimize_symbolic(target_math);

    // Read concrete matrix files
    auto [b_sizes, b_data] = read_matrix("examples/data/stochastic_newton_b.csv");
    auto [in_sizes, in_data] = read_matrix("examples/data/stochastic_newton_in.csv");
    auto [a_sizes, a_data] = read_matrix("examples/data/stochastic_newton_a.csv");
    auto [w_sizes, w_data] = read_matrix("examples/data/stochastic_newton_w.csv");
    auto [il_sizes, il_data] = read_matrix("examples/data/stochastic_newton_il.csv");

    if (b_sizes.first <= 0 || b_sizes.second <= 0 || b_sizes.first != b_sizes.second) {
        std::cerr << "Size error: B must be square.\n";
        return 1;
    }
    if (in_sizes != b_sizes) {
        std::cerr << "Size error: In must match dimensions of B.\n";
        return 1;
    }
    if (a_sizes.second != b_sizes.first) {
        std::cerr << "Size error: Columns of A must match rows of B.\n";
        return 1;
    }
    if (w_sizes.first != a_sizes.first) {
        std::cerr << "Size error: Rows of W_k must match rows of A.\n";
        return 1;
    }
    if (il_sizes.first != w_sizes.second || il_sizes.second != w_sizes.second) {
        std::cerr << "Size error: Il must be square with size equal to columns of W_k.\n";
        return 1;
    }

    // Concrete Size & Data Bindings (given evaluation sizes: b=1000, a=5000, l=625, k=10)
    SizeBindings concrete_sizes = {{"b", b_sizes.second}, {"a", a_sizes.first}, {"l", w_sizes.second}};
    DataBindings concrete_data = {{"B", b_data},   {"In", in_data}, {"A", a_data},
                                  {"W_k", w_data}, {"Il", il_data}, {"k", std::vector<double>{10.0}}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "examples/data/stochastic_newton_result.csv";
    write_matrix(out_path, row, col, out);
    return 0;
}
