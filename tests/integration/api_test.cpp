
#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
using namespace egraph;

using namespace EGraphRunner;

TEST(ApiTest, VariableConstruction) {
    Expression A("A");
    EXPECT_EQ(A.to_string(), "A");
}

TEST(ApiTest, Addition) {
    Expression A("A");
    Expression B("B");
    Expression C = A + B;
    EXPECT_EQ(C.to_string(), "A + B");
}

TEST(ApiTest, Multiplication) {
    Expression A("A");
    Expression B("B");
    Expression C = A * B;
    EXPECT_EQ(C.to_string(), "A * B");
}

TEST(ApiTest, ComplexExpression) {
    Expression A("A");
    Expression B("B");
    Expression C("C");
    // (A * B)^T + C
    Expression my_math = transpose(A * B) + C;
    EXPECT_EQ(my_math.to_string(), "Tr(A * B) + C");
}

TEST(ApiTest, ContextOptimization) {
    Context ctx;
    ctx.define_matrix("A", 100, 200);
    ctx.define_matrix("B", 200, 300);

    Expression A("A");
    Expression B("B");

    Expression my_math = transpose(A * B);

    Expression best_ast = ctx.optimize_concrete(my_math, {}, {"complete"});

    EXPECT_EQ(best_ast.to_string(true), "Gemm_TT(B, A, Zero_300x100)");
}

TEST(ApiTest, OLSSymbolic) {
    Context ctx((EGraph(get_property_table())));
    ctx.define_matrix("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix("n", "A", 1);

    Expression M("M");
    Expression n("n");

    // (M^T * M)^-1 * M^T * n
    Expression target_math = inverse(transpose(M) * M) * transpose(M) * n;

    Id target_id = ctx.add(target_math);

    ctx.get_config().rewrite.node_limit = 1000;
    ctx.get_config().rewrite.enable_backoff = true;
    ctx.get_config().rewrite.max_iterations = 10;
    ctx.rewrite({"property_discovery", "simplification", "transformation", "expansion"});

    SizeBindings bindings = {{"A", 30}, {"B", 10}};
    ExtractionResult best_result = ctx.extract(target_id, bindings);
    std::string actual = best_result.expr.to_string(true);
    std::string expected_cholel = "CholeL(Mᵀ * M)⁻¹ᵀ * (CholeL(Mᵀ * M)⁻¹ * (Mᵀ * n))";
    std::string expected_choleu = "CholeU(Mᵀ * M)⁻¹ * (CholeU(Mᵀ * M)⁻¹ᵀ * (Mᵀ * n))";
    EXPECT_TRUE(actual == expected_cholel || actual == expected_choleu) << "Actual expression: " << actual;
}

TEST(ApiTest, KernelMapping) {
    Context ctx((EGraph(get_property_table())));

    Expression X("X");
    Expression y("y");

    // (X^T * X)^-1 * X^T * y
    Expression target_math = inverse(transpose(X) * X) * transpose(X) * y;
    Expression best_ast = ctx.optimize_concrete(target_math);
    std::string actual = best_ast.to_string(true);
    std::string expected_qr = "Trsm_LN(R(X), Ormqr_LT(Geqrf(X), y))";
    std::string expected_qr2 = "Trsm_LN(R(X), Trsm_LT(R(X), Gemv_T(X, y, Zero_2x1)))";
    EXPECT_TRUE(actual == expected_qr || actual == expected_qr2) << "Actual: " << actual;
    auto res = ctx.evaluate_concrete(
        {}, {{"X", std::vector<double>{1.0, 0.0, 0.0, 0.0, 1.0, 0.0}}, {"y", std::vector<double>{5.0, -3.0, 42.0}}});
    ASSERT_EQ(res.size(), 2);
    EXPECT_NEAR(res[0], 5.0, 1e-6);
    EXPECT_NEAR(res[1], -3.0, 1e-6);
}

TEST(ApiTest, OptimizeSymbolic) {
    Context ctx((EGraph(get_property_table())));

    ctx.define_matrix("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix("n", "A", 1);
    Expression M("M");
    Expression n("n");

    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    ctx.optimize_symbolic(target_math);
    auto results = ctx.extract_symbolic();

    bool found = std::any_of(results.begin(), results.end(), [](const auto &c) {
        std::string s = c.expr.to_string(false);
        return s == "Trsm_LT(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), Trsm_LN(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), "
                    "Gemv_T(M, n, Zero_Bx1)))" ||
               s == "Trsm_LN(Get(Potrf_U(Syrk_T(M, Zero_BxB)), 0), Trsm_LT(Get(Potrf_U(Syrk_T(M, Zero_BxB)), 0), "
                    "Gemv_T(M, n, Zero_Bx1)))";
    });
    EXPECT_TRUE(found);

    SizeBindings concrete_sizes = {{"A", 3}, {"B", 2}};

    DataBindings data1 = {
        {"M", std::vector<double>{1.0, 0.0, 0.0, 0.0, 1.0, 0.0}}, {"n", std::vector<double>{5.0, -3.0, 42.0}}};
    auto out1 = ctx.evaluate_concrete(concrete_sizes, data1);

    ASSERT_EQ(out1.size(), 2);
    EXPECT_NEAR(out1[0], 5.0, 1e-6);
    EXPECT_NEAR(out1[1], -3.0, 1e-6);

    DataBindings data2 = {
        {"M", std::vector<double>{2.0, 1.0, 0.0, 1.0, 3.0, 1.0}}, {"n", std::vector<double>{5.0, 10.0, 3.0}}};
    auto out2 = ctx.evaluate_concrete(concrete_sizes, data2);

    ASSERT_EQ(out2.size(), 2);
    EXPECT_NEAR(out2[0], 1.0, 1e-6);
    EXPECT_NEAR(out2[1], 3.0, 1e-6);
}

TEST(ApiTest, ConcreteMatrixChainEvaluation) {
    Context ctx;
    ctx.define_matrix("A", 2, 2);
    ctx.define_matrix("B", 2, 2);
    ctx.define_matrix("C", 2, 2);

    Expression A("A");
    Expression B("B");
    Expression C("C");

    Expression target = (A + B) * C;
    ctx.optimize_concrete(target);

    // A = [1 0; 0 1], B = [2 0; 0 2], C = [4 1; 2 5]
    // (A + B) * C = 3 * C = [12 3; 6 15]
    // in col-major: col 0 = [12, 6], col 1 = [3, 15]
    DataBindings data = {{"A", {1.0, 0.0, 0.0, 1.0}}, {"B", {2.0, 0.0, 0.0, 2.0}}, {"C", {4.0, 2.0, 1.0, 5.0}}};

    auto res = ctx.evaluate_concrete({}, data);
    ASSERT_EQ(res.size(), 4);
    EXPECT_NEAR(res[0], 12.0, 1e-6);
    EXPECT_NEAR(res[1], 6.0, 1e-6);
    EXPECT_NEAR(res[2], 3.0, 1e-6);
    EXPECT_NEAR(res[3], 15.0, 1e-6);
}

TEST(ApiTest, SymbolicSyrkPipelineDemonstration) {
    Context ctx;
    ctx.get_config().enable_logging = false;

    // 1. Symbolic Matrix Definition: A has symbolic shape M x K and is tall
    ctx.define_matrix("A", "M", "K", {"tall"});

    Expression A("A");
    Expression target_math = transpose(A) * A;

    Id root_id = ctx.add(target_math);

    // Initial E-Graph: 3 nodes (A, Tr, *), 3 classes
    EXPECT_EQ(ctx.get_egraph().num_nodes(), 3);
    EXPECT_EQ(ctx.get_egraph().get_all_class_ids().size(), 3);
    ctx.get_egraph().to_img("syrk_symbolic_1_initial", "svg");

    // 2. Rewrite & Lowering (small rewrite budget: 1 iteration)
    ctx.get_config().rewrite.max_iterations = 1;
    ctx.get_config().rewrite.enable_backoff = false;
    ctx.get_config().rewrite.enable_node_limit = false;
    ctx.rewrite({"property_discovery", "simplification", "transformation"});

    std::vector<Rewrite> lowering_rewrites = build_rewrite_sets({"lowering"});
    Rewriter rewriter(ctx.get_egraph(), lowering_rewrites, ctx.get_config());
    rewriter.apply_rewrites();

    // Saturated & Lowered E-Graph: 7 nodes, 4 classes
    // Root class contains alternative implementations: *, Gemm_NN, Gemm_TN, Syrk_T
    EXPECT_EQ(ctx.get_egraph().num_nodes(), 7);
    EXPECT_EQ(ctx.get_egraph().get_all_class_ids().size(), 4);

    Id root_class = ctx.get_egraph().find_class_id(root_id);
    const auto &root_nodes = ctx.get_egraph().get_class_nodes(root_class);
    bool has_syrk = std::any_of(root_nodes.begin(), root_nodes.end(), [](const ENode *node) {
        return node->to_string() == "Syrk_T";
    });
    EXPECT_TRUE(has_syrk);
    ctx.get_egraph().to_img("syrk_symbolic_2_lowered", "svg");

    // 3. Pruning: eliminate symbolic ops and suboptimal kernels
    Pruner::prune_symbolic_when_kernel_available(ctx.get_egraph(), {root_id});

    Extractor extractor(ctx.get_egraph(), ctx.get_config());
    Pruner pruner(ctx.get_egraph(), extractor);
    auto bindings = sample_size_bindings(10, 10, 5000, {"M", "K"}, 99, &ctx.get_egraph().get_property_table());
    pruner.prune({root_id}, bindings);
    Pruner::eliminate_unreachable_classes(ctx.get_egraph(), {root_id});

    // Pruned E-Graph: compressed to 3 nodes (A, Zero_KxK, Syrk_T), 3 classes
    EXPECT_EQ(ctx.get_egraph().num_nodes(), 3);
    EXPECT_EQ(ctx.get_egraph().get_all_class_ids().size(), 3);
    ctx.get_egraph().to_img("syrk_symbolic_3_pruned", "svg");

    // Verify optimal extraction
    auto sym_results = ctx.extract_symbolic(root_id);
    ASSERT_FALSE(sym_results.empty());
    EXPECT_EQ(sym_results[0].expr.to_string(false), "Syrk_T(A, Zero_KxK)");
}