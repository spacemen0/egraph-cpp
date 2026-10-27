
#include "api.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

#include <cstddef>
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

TEST(ApiTest, ContextRegisterKernelWithCost) {
    Context ctx;
    Op custom_kernel = ctx.register_kernel("CustomKernel", [](const std::vector<Operand> &) {
        MatrixProperty p;
        p.shape = Shape{2, 2};
        return AnalysisData{p};
    }, [](Op, const std::vector<Operand> &) -> Cost {
        return 10.0;
    });

    EXPECT_TRUE(is_kernel_op(custom_kernel));
    const auto &desc = OpRegistry::instance().get(custom_kernel);
    EXPECT_NE(desc.cost, nullptr);
}

TEST(ApiTest, ContextCustomKernelEvaluation) {
    Context ctx;
    Op double_kernel = ctx.register_kernel("DoubleElements", [](const std::vector<Operand> &operands) {
        const auto *m = as_matrix(operands[0]);
        MatrixProperty p = m->property;
        return AnalysisData{p};
    }, [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[0]);
        return static_cast<double>(m->rows * m->cols);
    }, [](const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs, MatrixNode &output) {
        const double *in = inputs[0]->data();
        double *out = output.data();
        size_t n = static_cast<long>(output.rows) * output.cols;
        for (size_t i = 0; i < n; ++i) {
            out[i] = in[i] * 2.0;
        }
    });

    auto a = ctx.define_matrix("A", 2, 2);
    Expression custom_expr(double_kernel, {a});
    ctx.optimize_concrete(custom_expr);

    DataBindings bindings;
    bindings["A"] = {1.0, 2.0, 3.0, 4.0};

    auto res = ctx.evaluate_concrete({}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 2.0);
    EXPECT_DOUBLE_EQ(res[1], 4.0);
    EXPECT_DOUBLE_EQ(res[2], 6.0);
    EXPECT_DOUBLE_EQ(res[3], 8.0);
}

TEST(ApiTest, ContextCustomInPlaceKernelEvaluation) {
    Context ctx;
    Op add_to_kernel = ctx.register_kernel(
        "AddToB",
        [](const std::vector<Operand> &operands) {
        const auto *m = as_matrix(operands[1]);
        MatrixProperty p = m->property;
        return AnalysisData{p};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[1]);
        return static_cast<double>(m->rows * m->cols);
    },
        [](const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs, MatrixNode &output) {
        const double *a = inputs[0]->data();
        double *b = output.data();
        size_t n = output.rows * output.cols;
        for (size_t i = 0; i < n; ++i) {
            b[i] += a[i];
        }
    },
        /*in_place_input=*/1);

    auto a = ctx.define_matrix("A", 2, 2);
    auto b = ctx.define_matrix("B", 2, 2);
    Expression custom_expr(add_to_kernel, {a, b});
    ctx.optimize_concrete(custom_expr);

    DataBindings bindings;
    bindings["A"] = {1.0, 2.0, 3.0, 4.0};
    bindings["B"] = {10.0, 20.0, 30.0, 40.0};

    auto res = ctx.evaluate_concrete({}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 11.0);
    EXPECT_DOUBLE_EQ(res[1], 22.0);
    EXPECT_DOUBLE_EQ(res[2], 33.0);
    EXPECT_DOUBLE_EQ(res[3], 44.0);
}