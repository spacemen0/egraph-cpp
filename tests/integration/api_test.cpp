
#include "api.h"
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
    Op double_kernel = ctx.register_kernel(
        "DoubleElements",
        [](const std::vector<Operand> &operands) {
        const auto *m = as_matrix(operands[0]);
        MatrixProperty p = m->property;
        return AnalysisData{p};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[0]);
        return static_cast<double>(m->rows * m->cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs,
           MatrixNode &output) {
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
        [](std::string_view, const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs,
           MatrixNode &output) {
        const double *a = inputs[0]->data();
        double *b = output.data();
        size_t n = static_cast<long>(output.rows) * output.cols;
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

TEST(ApiTest, ContextCustomTupleProducingKernel) {
    Context ctx;
    Op split_kernel = ctx.register_kernel(
        "SplitUpperLower",
        [](const std::vector<Operand> &operands) -> AnalysisData {
        const auto *m = as_matrix(operands[0]);
        MatrixProperty upper = m->property;
        upper.flags.is_upper_triangular = true;
        MatrixProperty lower = m->property;
        lower.flags.is_lower_triangular = true;
        return AnalysisData{TupleProperty{upper, lower}};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[0]);
        return static_cast<double>(m->rows * m->cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const EvalValue *> &inputs,
           EvalValue &output) {
        const auto &in_mat = std::get<MatrixNode>(*inputs[0]);
        auto &tuple = std::get<TupleNode>(output);
        MatrixNode &upper = tuple.matrices[0];
        MatrixNode &lower = tuple.matrices[1];
        int n = in_mat.rows;
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                double val = in_mat.data()[i + j * n];
                upper.data()[i + j * n] = (i <= j) ? val : 0.0;
                lower.data()[i + j * n] = (i >= j) ? val : 0.0;
            }
        }
    });

    auto a = ctx.define_matrix("A", 2, 2);
    Expression split_expr(split_kernel, {a});
    Expression get_upper(OpRegistry::op("Get"), {split_expr, Expression(0)});
    Expression get_lower(OpRegistry::op("Get"), {split_expr, Expression(1)});
    Expression sum_expr = get_upper + get_lower;
    ctx.optimize_concrete(sum_expr);

    DataBindings bindings;
    // A = [1 2; 3 4] in col-major: [1.0, 3.0, 2.0, 4.0]
    // upper: [1 2; 0 4] -> col-major: [1, 0, 2, 4]
    // lower: [1 0; 3 4] -> col-major: [1, 3, 0, 4]
    // sum: [2 2; 3 8] -> col-major: [2, 3, 2, 8]
    bindings["A"] = {1.0, 3.0, 2.0, 4.0};

    auto res = ctx.evaluate_concrete({}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 2.0);
    EXPECT_DOUBLE_EQ(res[1], 3.0);
    EXPECT_DOUBLE_EQ(res[2], 2.0);
    EXPECT_DOUBLE_EQ(res[3], 8.0);
}

TEST(ApiTest, ContextCustomTupleConsumingKernel) {
    Context ctx;
    Op split_kernel = ctx.register_kernel(
        "SplitDiagOffDiag",
        [](const std::vector<Operand> &operands) -> AnalysisData {
        const auto *m = as_matrix(operands[0]);
        return AnalysisData{TupleProperty{m->property, m->property}};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[0]);
        return static_cast<double>(m->rows * m->cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const EvalValue *> &inputs,
           EvalValue &output) {
        const auto &in_mat = std::get<MatrixNode>(*inputs[0]);
        auto &tuple = std::get<TupleNode>(output);
        MatrixNode &diag = tuple.matrices[0];
        MatrixNode &offdiag = tuple.matrices[1];
        int n = in_mat.rows;
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                double val = in_mat.data()[i + j * n];
                diag.data()[i + j * n] = (i == j) ? val : 0.0;
                offdiag.data()[i + j * n] = (i != j) ? val : 0.0;
            }
        }
    });

    Op combine_kernel = ctx.register_kernel(
        "CombineTupleDiff",
        [](const std::vector<Operand> &operands) -> AnalysisData {
        const auto *t = as_tuple(operands[0]);
        return AnalysisData{(*t)[0].property};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *t = as_tuple(operands[0]);
        return static_cast<double>((*t)[0].rows * (*t)[0].cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const EvalValue *> &inputs,
           EvalValue &output) {
        const auto &tuple = std::get<TupleNode>(*inputs[0]);
        auto &out_mat = std::get<MatrixNode>(output);
        const auto &diag = tuple.matrices[0];
        const auto &offdiag = tuple.matrices[1];
        for (size_t i = 0; i < out_mat.vec().size(); ++i) {
            out_mat.data()[i] = diag.data()[i] - offdiag.data()[i];
        }
    });

    auto a = ctx.define_matrix("A", 2, 2);
    Expression split_expr(split_kernel, {a});
    Expression combine_expr(combine_kernel, {split_expr});
    ctx.optimize_concrete(combine_expr);

    DataBindings bindings;
    // A = [1 2; 3 4] -> diag = [1 0; 0 4], offdiag = [0 2; 3 0]
    // diag - offdiag = [1 -2; -3 4] -> col-major: [1.0, -3.0, -2.0, 4.0]
    bindings["A"] = {1.0, 3.0, 2.0, 4.0};

    auto res = ctx.evaluate_concrete({}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 1.0);
    EXPECT_DOUBLE_EQ(res[1], -3.0);
    EXPECT_DOUBLE_EQ(res[2], -2.0);
    EXPECT_DOUBLE_EQ(res[3], 4.0);
}

TEST(ApiTest, ContextCustomInPlaceTupleKernel) {
    Context ctx;
    Op in_place_tuple_kernel = ctx.register_kernel(
        "InPlaceTupleScale",
        [](const std::vector<Operand> &operands) -> AnalysisData {
        const auto *m = as_matrix(operands[0]);
        return AnalysisData{TupleProperty{m->property}};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *m = as_matrix(operands[0]);
        return static_cast<double>(m->rows * m->cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const EvalValue *> &, EvalValue &output) {
        auto &tuple = std::get<TupleNode>(output);
        MatrixNode &m = tuple.matrices[0];
        for (size_t i = 0; i < m.vec().size(); ++i) {
            m.data()[i] *= 10.0;
        }
    },
        /*in_place_input=*/0);

    auto a = ctx.define_matrix("A", 2, 2);
    Expression factor_expr(in_place_tuple_kernel, {a});
    Expression get_expr(OpRegistry::op("Get"), {factor_expr, Expression(0)});
    ctx.optimize_concrete(get_expr);

    DataBindings bindings;
    bindings["A"] = {1.0, 2.0, 3.0, 4.0};

    auto res = ctx.evaluate_concrete({}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 10.0);
    EXPECT_DOUBLE_EQ(res[1], 20.0);
    EXPECT_DOUBLE_EQ(res[2], 30.0);
    EXPECT_DOUBLE_EQ(res[3], 40.0);
}

TEST(ApiTest, SimpleAdd) {
    Context ctx;

    Op simple_add = ctx.register_kernel(
        "SimpleAdd",
        [](const std::vector<Operand> &operands) -> AnalysisData {
        return AnalysisData{as_matrix(operands[0])->property};
    },
        [](Op, const std::vector<Operand> &operands) -> Cost {
        const auto *a = as_matrix(operands[0]);
        // Give it lower cost than default Add
        return 0.5 * static_cast<double>(a->rows * a->cols);
    },
        [](std::string_view, const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs,
           MatrixNode &output) {
        const double *a = inputs[0]->data();
        const double *b = inputs[1]->data();
        double *out = output.data();
        size_t n = static_cast<size_t>(output.rows) * output.cols;
        for (size_t i = 0; i < n; ++i) {
            out[i] = a[i] + b[i];
        }
    });

    // 2. Define a rewrite rule lowering symbolic addition to SimpleAdd
    Rewrite lower_add_rule = make_rewrite("lower_to_simple_add", "?a + ?b", "SimpleAdd(?a, ?b)", false);

    // 3. Construct the high-level symbolic expression A + B
    auto A = ctx.define_matrix("A", 2, 2);
    auto B = ctx.define_matrix("B", 2, 2);
    Expression high_level_expr = A + B;
    Id root_id = ctx.add(high_level_expr);

    // 4. Apply the rewrite rule to discover the kernel implementation in the egraph
    ctx.apply_rewrites({lower_add_rule});

    // 5. Extract the cost-optimal expression (SimpleAdd has lower cost than Add)
    auto extraction_result = ctx.extract(root_id, {});
    EXPECT_EQ(extraction_result.expr.to_string(), "SimpleAdd(A, B)");

    // 6. Evaluate with concrete numeric data bindings
    DataBindings bindings;
    bindings["A"] = {1.0, 2.0, 3.0, 4.0};
    bindings["B"] = {10.0, 20.0, 30.0, 40.0};

    auto res = ctx.evaluate_concrete(root_id, {}, bindings);
    ASSERT_EQ(res.size(), 4);
    EXPECT_DOUBLE_EQ(res[0], 11.0);
    EXPECT_DOUBLE_EQ(res[1], 22.0);
    EXPECT_DOUBLE_EQ(res[2], 33.0);
    EXPECT_DOUBLE_EQ(res[3], 44.0);
}
