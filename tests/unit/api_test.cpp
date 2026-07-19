#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

using namespace egraph;

TEST(ApiTest, VariableConstruction) {
    Expr A("A");
    EXPECT_EQ(A.ast.to_string(), "A");
}

TEST(ApiTest, Addition) {
    Expr A("A");
    Expr B("B");
    Expr C = A + B;
    EXPECT_EQ(C.ast.to_string(), "A + B");
}

TEST(ApiTest, Multiplication) {
    Expr A("A");
    Expr B("B");
    Expr C = A * B;
    EXPECT_EQ(C.ast.to_string(), "A * B");
}

TEST(ApiTest, ComplexExpression) {
    Expr A("A");
    Expr B("B");
    Expr C("C");
    // (A * B)^T + C
    Expr my_math = transpose(A * B) + C;
    EXPECT_EQ(my_math.ast.to_string(), "Tr(A * B) + C");
}

TEST(ApiTest, ContextOptimization) {
    Context ctx;
    ctx.define_matrix("A", 100, 200);
    ctx.define_matrix("B", 200, 300);

    Expr A("A");
    Expr B("B");

    // (A * B)^T -> B^T * A^T
    Expr my_math = transpose(A * B);

    // Using complete ruleset which should include the transpose distribution rule
    Expression best_ast = ctx.optimize_concrete(my_math, {}, {"complete"});

    // The optimized form should distribute the transpose and swap the order, then lower to Gemm_NN
    EXPECT_EQ(best_ast.to_string(true), "Gemm_NN(Bᵀ, Aᵀ, Zero_300x100)");
}

TEST(ApiTest, OLSSymbolic) {
    Context ctx;
    ctx.egraph = EGraph(get_property_table());
    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);

    Expr M("M");
    Expr n("n");

    // (M^T * M)^-1 * M^T * n
    Expr target_math = inverse(transpose(M) * M) * transpose(M) * n;

    // Using granular control as an example
    Id target_id = ctx.add(target_math);

    // Run iterations exactly like the original integration test
    ctx.rewrite({"simplification", "transformation", "expansion"}, 1000, true, 10);

    // Provide the symbolic size bindings for the extract phase
    SizeBindings bindings = {{"A", 30}, {"B", 10}};
    Expression best_ast = ctx.extract(target_id, bindings);

    EXPECT_EQ(best_ast.to_string(true), "Sol(R(M), Q(M)ᵀ * n)");
}

TEST(ApiTest, KernelMapping) {
    Context ctx;
    ctx.egraph = EGraph(get_property_table());

    Expr X("X");
    Expr y("y");

    // (X^T * X)^-1 * X^T * y
    Expr target_math = inverse(transpose(X) * X) * transpose(X) * y;
    Expression best_ast = ctx.optimize_concrete(target_math);

    EXPECT_EQ(best_ast.to_string(true), "Trsm_LN(R(X), Gemv_T(Q(X), y, Zero_2x1))");
}

TEST(ApiTest, OptimizeSymbolic) {
    Context ctx;
    ctx.egraph = EGraph(get_property_table());

    ctx.define_matrix_symbolic("M", "A", "B", {"positive_definite", "tall"});
    Expr M("M");
    Expr n("n");

    Expr target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    Id target_id = ctx.optimize_symbolic(target_math, {"A", "B"});
    auto results = ctx.extract_symbolic(target_id);

    bool found = std::any_of(results.begin(), results.end(), [](const auto &c) {
        return c.expr.to_string(true) == "Trsm_LN(R(M), Gemv_T(Q(M), n, Zero_Bx1))";
    });
    EXPECT_TRUE(found);

    SizeBindings concrete_sizes = {{"A", 30}, {"B", 10}};
    Expression concrete_ast = ctx.extract(target_id, concrete_sizes);
    EXPECT_FALSE(concrete_ast.to_string(true).empty());
}
