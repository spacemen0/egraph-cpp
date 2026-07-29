#include "evaluator.h"

#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <iostream>

using namespace egraph;

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
    Context ctx;
    ctx.egraph = EGraph(get_property_table());
    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);

    Expression M("M");
    Expression n("n");

    // (M^T * M)^-1 * M^T * n
    Expression target_math = inverse(transpose(M) * M) * transpose(M) * n;

    Id target_id = ctx.add(target_math);

    ctx.rewrite({"simplification", "transformation", "expansion"}, 1000, true, 10);

    SizeBindings bindings = {{"A", 30}, {"B", 10}};
    ExtractionResult best_result = ctx.extract(target_id, bindings);

    EXPECT_EQ(best_result.expr.to_string(true), "Sol(R(M), Q(M)ᵀ * n)");
}

TEST(ApiTest, KernelMapping) {
    Context ctx;
    ctx.egraph = EGraph(get_property_table());

    Expression X("X");
    Expression y("y");

    // (X^T * X)^-1 * X^T * y
    Expression target_math = inverse(transpose(X) * X) * transpose(X) * y;
    Expression best_ast = ctx.optimize_concrete(target_math);

    EXPECT_EQ(best_ast.to_string(true), "Trsm_LN(R(X), Gemv_T(Q(X), y, Zero_2x1))");
}

TEST(ApiTest, OptimizeSymbolic) {
    Context ctx;
    ctx.egraph = EGraph(get_property_table());

    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    Expression M("M");
    Expression n("n");

    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    Id target_id = ctx.optimize_symbolic(target_math, {"A", "B"});
    auto results = ctx.extract_symbolic(target_id);

    bool found = std::any_of(results.begin(), results.end(), [](const auto &c) {
        std::cout << "CANDIDATE: " << c.expr.to_string(false) << std::endl;
        return c.expr.to_string(false) == "Trsm_LN(Get(Potrf_U(Syrk_T(M, Zero_BxB)), 0), Trsm_LT(Get(Potrf_U(Syrk_T(M, "
                                          "Zero_BxB)), 0), Gemv_T(M, n, Zero_Bx1)))" ||
               c.expr.to_string(false) == "Trsm_LT(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), Trsm_LN(Get(Potrf_L(Syrk_T(M, "
                                          "Zero_BxB)), 0), Gemv_T(M, n, Zero_Bx1)))";
    });
    EXPECT_TRUE(found);

    SizeBindings concrete_sizes = {{"A", 30}, {"B", 10}};
    ExtractionResult concrete_result = ctx.extract(target_id, concrete_sizes);
    std::cout << "Concrete result: " << concrete_result.expr.to_string(false) << std::endl;
    EXPECT_FALSE(concrete_result.expr.to_string(false).empty());

    Evaluator evaluator(ctx.egraph, concrete_result, &concrete_sizes);
    const auto &out = evaluator.evaluate();
    EXPECT_FALSE(out.empty());
}
