
#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <iostream>
#include <random>

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
    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);

    Expression M("M");
    Expression n("n");

    // (M^T * M)^-1 * M^T * n
    Expression target_math = inverse(transpose(M) * M) * transpose(M) * n;

    Id target_id = ctx.add(target_math);

    ctx.config.rewrite.node_limit = 1000;
    ctx.config.rewrite.enable_backoff = true;
    ctx.config.rewrite.max_iterations = 10;
    ctx.rewrite({"simplification", "transformation", "expansion"});

    SizeBindings bindings = {{"A", 30}, {"B", 10}};
    ExtractionResult best_result = ctx.extract(target_id, bindings);

    EXPECT_EQ(best_result.expr.to_string(true), "Sol(LLt(SymMul(M))ᵀ, Sol(LLt(SymMul(M)), Mᵀ * n))");
}

TEST(ApiTest, KernelMapping) {
    Context ctx((EGraph(get_property_table())));

    Expression X("X");
    Expression y("y");

    // (X^T * X)^-1 * X^T * y
    Expression target_math = inverse(transpose(X) * X) * transpose(X) * y;
    Expression best_ast = ctx.optimize_concrete(target_math);
    EXPECT_EQ(best_ast.to_string(true), "Trsm_LN(R(X), Ormqr_LT(Geqrf(X), y))");
    ctx.evaluate_concrete(
        {}, {{"X", std::vector<double>{1.0, 0.0, 0.0, 0.0, 1.0, 0.0}}, {"y", std::vector<double>{5.0, -3.0, 42.0}}});
}

TEST(ApiTest, OptimizeSymbolic) {
    Context ctx((EGraph(get_property_table())));

    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);
    Expression M("M");
    Expression n("n");

    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    ctx.optimize_symbolic(target_math);
    auto results = ctx.extract_symbolic();

    bool found = std::any_of(results.begin(), results.end(), [](const auto &c) {
        return c.expr.to_string(false) ==
               "Trsm_LT(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), Trsm_LN(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), Gemv_T(M, n, Zero_Bx1)))";
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

TEST(ApiTest, EvaluateConcrete) {
    Context ctx;
    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);
    Expression M("M");
    Expression n("n");

    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    ctx.optimize_symbolic(target_math);

    SizeBindings concrete_sizes = {{"A", 30}, {"B", 20}};
    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist(-5.0, 5.0);

    DataBindings concrete_data = {
        {"M", std::vector<double>(concrete_sizes["A"] * concrete_sizes["B"])},
        {"n", std::vector<double>(concrete_sizes["A"])}};

    for (auto &value : concrete_data["M"]) {
        value = dist(gen);
    }
    for (auto &value : concrete_data["n"]) {
        value = dist(gen);
    }

    auto out1 = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    for (size_t i = 0; i < out1.size(); ++i) {
        std::cout << out1[i] << (i == out1.size() - 1 ? "" : " ");
    }
}