#include "api.h"
#include "evaluator.h"
#include "extractor.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

using namespace egraph;

TEST(MatrixNodeTest, DefaultConstructor) {
    MatrixNode node;
    EXPECT_EQ(node.rows, 0);
    EXPECT_EQ(node.cols, 0);
    EXPECT_EQ(node.format, StorageFormat::General);
    EXPECT_EQ(node.data(), nullptr);
}

TEST(MatrixNodeTest, DimensionConstructor) {
    MatrixNode node(3, 4);
    EXPECT_EQ(node.rows, 3);
    EXPECT_EQ(node.cols, 4);
    EXPECT_EQ(node.format, StorageFormat::General);
    ASSERT_NE(node.data(), nullptr);
    EXPECT_EQ(node.vec().size(), 12);
}

TEST(MatrixNodeTest, VectorConstructor) {
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    MatrixNode node(2, 3, vals);
    EXPECT_EQ(node.rows, 2);
    EXPECT_EQ(node.cols, 3);
    EXPECT_EQ(node.vec(), vals);
}

TEST(MatrixNodeTest, FormatConstructor) {
    MatrixNode node(3, 3, StorageFormat::SymmetricUpper);
    EXPECT_EQ(node.rows, 3);
    EXPECT_EQ(node.cols, 3);
    EXPECT_EQ(node.format, StorageFormat::SymmetricUpper);
}

TEST(MatrixNodeTest, EnsureGeneralFromGeneralIsNoOp) {
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0};
    MatrixNode node(2, 2, vals);
    node.ensure_general();
    EXPECT_EQ(node.format, StorageFormat::General);
    EXPECT_EQ(node.vec(), vals);
}

TEST(MatrixNodeTest, EnsureGeneralSymmetricUpper) {
    // 2x2 column-major matrix:
    // [1.0, 5.0]
    // [9.0, 2.0]  (where 9.0 is garbage below diagonal)
    // col-major order: (0,0)=1.0, (1,0)=9.0, (0,1)=5.0, (1,1)=2.0
    std::vector<double> vals = {1.0, 9.0, 5.0, 2.0};
    MatrixNode node(2, 2, vals);
    node.format = StorageFormat::SymmetricUpper;

    node.ensure_general();

    EXPECT_EQ(node.format, StorageFormat::General);
    // Upper value (0,1)=5.0 should be copied to (1,0)
    std::vector<double> expected = {1.0, 5.0, 5.0, 2.0};
    EXPECT_EQ(node.vec(), expected);
}

TEST(MatrixNodeTest, EnsureGeneralSymmetricLower) {
    // 2x2 column-major matrix:
    // [1.0, 9.0]  (where 9.0 is garbage above diagonal)
    // [4.0, 2.0]
    // col-major order: (0,0)=1.0, (1,0)=4.0, (0,1)=9.0, (1,1)=2.0
    std::vector<double> vals = {1.0, 4.0, 9.0, 2.0};
    MatrixNode node(2, 2, vals);
    node.format = StorageFormat::SymmetricLower;

    node.ensure_general();

    EXPECT_EQ(node.format, StorageFormat::General);
    // Lower value (1,0)=4.0 should be copied to (0,1)
    std::vector<double> expected = {1.0, 4.0, 4.0, 2.0};
    EXPECT_EQ(node.vec(), expected);
}

TEST(MatrixNodeTest, EnsureGeneralTriangularUpper) {
    // 2x2 column-major:
    // [1.0, 3.0]
    // [9.0, 2.0]
    // Lower triangle (1,0) should become 0.0
    std::vector<double> vals = {1.0, 9.0, 3.0, 2.0};
    MatrixNode node(2, 2, vals);
    node.format = StorageFormat::TriangularUpper;

    node.ensure_general();

    EXPECT_EQ(node.format, StorageFormat::General);
    std::vector<double> expected = {1.0, 0.0, 3.0, 2.0};
    EXPECT_EQ(node.vec(), expected);
}

TEST(MatrixNodeTest, EnsureGeneralTriangularLower) {
    // 2x2 column-major:
    // [1.0, 9.0]
    // [3.0, 2.0]
    // Upper triangle (0,1) should become 0.0
    std::vector<double> vals = {1.0, 3.0, 9.0, 2.0};
    MatrixNode node(2, 2, vals);
    node.format = StorageFormat::TriangularLower;

    node.ensure_general();

    EXPECT_EQ(node.format, StorageFormat::General);
    std::vector<double> expected = {1.0, 3.0, 0.0, 2.0};
    EXPECT_EQ(node.vec(), expected);
}

TEST(EvaluatorTest, EvaluateMatrixAddition) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("A", 2, 2);
    ctx.define_matrix("B", 2, 2);

    Expression A("A");
    Expression B("B");
    ctx.optimize_concrete(A + B);

    DataBindings data = {
        {"A", {1.0, 2.0, 3.0, 4.0}},
        {"B", {10.0, 20.0, 30.0, 40.0}}
    };

    auto result = ctx.evaluate_concrete({}, data);
    ASSERT_EQ(result.size(), 4);
    EXPECT_DOUBLE_EQ(result[0], 11.0);
    EXPECT_DOUBLE_EQ(result[1], 22.0);
    EXPECT_DOUBLE_EQ(result[2], 33.0);
    EXPECT_DOUBLE_EQ(result[3], 44.0);
}

TEST(EvaluatorTest, EvaluateMatrixMultiplication) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("A", 2, 3, {"full_rank"});
    ctx.define_matrix("B", 3, 2, {"full_rank"});

    Expression A("A");
    Expression B("B");
    ctx.optimize_concrete(A * B);

    DataBindings data = {
        {"A", {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
        {"B", {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}}
    };

    auto result = ctx.evaluate_concrete({}, data);
    ASSERT_EQ(result.size(), 4);
    EXPECT_NEAR(result[0], 22.0, 1e-9);
    EXPECT_NEAR(result[1], 28.0, 1e-9);
    EXPECT_NEAR(result[2], 49.0, 1e-9);
    EXPECT_NEAR(result[3], 64.0, 1e-9);
}

TEST(EvaluatorTest, EvaluateMatrixVectorMultiplication) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("A", 2, 2);
    ctx.define_matrix("x", 2, 1);

    Expression A("A");
    Expression x("x");
    ctx.optimize_concrete(A * x);

    DataBindings data = {
        {"A", {2.0, 1.0, 0.0, 3.0}},
        {"x", {4.0, 5.0}}
    };

    auto result = ctx.evaluate_concrete({}, data);
    ASSERT_EQ(result.size(), 2);
    EXPECT_NEAR(result[0], 8.0, 1e-9);
    EXPECT_NEAR(result[1], 19.0, 1e-9);
}

TEST(EvaluatorTest, MissingDataBindingThrowsRuntimeError) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("A", 2, 2);
    ctx.define_matrix("B", 2, 2);

    Expression A("A");
    Expression B("B");
    ctx.optimize_concrete(A + B);

    DataBindings incomplete_data = {
        {"A", {1.0, 2.0, 3.0, 4.0}}
    };

    EXPECT_THROW(ctx.evaluate_concrete({}, incomplete_data), std::runtime_error);
}

TEST(EvaluatorTest, PrintExecutionPlanOutputsDetails) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("A", 2, 2);
    ctx.define_matrix("B", 2, 2);

    Expression A("A");
    Expression B("B");
    ctx.optimize_concrete(A + B);

    DataBindings data = {
        {"A", {1.0, 2.0, 3.0, 4.0}},
        {"B", {5.0, 6.0, 7.0, 8.0}}
    };

    std::stringstream buffer;
    std::streambuf *old_cout = std::cout.rdbuf(buffer.rdbuf());
    ctx.evaluate_concrete({}, data);
    std::cout.rdbuf(old_cout);

    std::string plan_output = buffer.str();
    EXPECT_FALSE(plan_output.empty());
    EXPECT_TRUE(plan_output.find("id:") != std::string::npos);
}

TEST(EvaluatorTest, MissingSizeBindingsThrowsRuntimeErrorInsteadOfCrashing) {
    EGraphRunner::Context ctx;
    ctx.define_matrix("M", "Matrix(m x n)");
    Expression M("M");
    ctx.optimize_concrete(M);

    DataBindings data = {
        {"M", {1.0, 2.0, 3.0, 4.0}}
    };
    SizeBindings incomplete_sizes;
    EXPECT_THROW(ctx.evaluate_concrete(incomplete_sizes, data), std::runtime_error);
}
