#include "api.h"
#include "test_helpers.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

using namespace egraph;

TEST(Integration, StochasticNewtonPruneRetainsCholesky) {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;

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

    size_t total_pruned = 0;
    Id root_id = ctx.get_egraph().add_expression(target_math);
    auto config = initialize_config_for_expression(target_math);
    config.enable_logging = false;
    ctx.get_config() = config;

    ctx.rewrite_and_prune(
        {root_id}, {"everything_but_lowering"}, nullptr,
        [&](int iteration, const PruneResult &res) { total_pruned += res.nodes_pruned; });

    EXPECT_GT(total_pruned, 500);

    ctx.lower_to_kernels({root_id});

    SizeBindings concrete_sizes = {{"b", 2000}, {"a", 4000}, {"l", 500}};
    auto result = ctx.extract(root_id, concrete_sizes);
    std::string expr_str = result.expr.to_string(false);

    // Verify that the pruner preserved the shared Cholesky factorization
    // and lowered it to Potrf_L, without leaving any unhandled generic Inv kernels
    EXPECT_TRUE(expr_str.find("Potrf_L") != std::string::npos) << "Expected Potrf_L in: " << expr_str;
    EXPECT_TRUE(expr_str.find("Inv(") == std::string::npos) << "Unexpected Inv in: " << expr_str;
}
