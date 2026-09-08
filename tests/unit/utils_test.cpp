#include "basic_types.h"
#include "errors.h"
#include "test_helpers.h"
#include "utils.h"
#include <gtest/gtest.h>

using namespace egraph;

TEST(UtilsTest, TrimWhitespace) {
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
    EXPECT_EQ(trim("\t\n\r  \v\f"), "");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim("  hello"), "hello");
    EXPECT_EQ(trim("hello  "), "hello");
    EXPECT_EQ(trim(" \t hello world \n\r "), "hello world");
}

TEST(UtilsTest, ParseOpValid) {
    EXPECT_EQ(parse_op("+"), Op::Add);
    EXPECT_EQ(parse_op("*"), Op::Mul);
    EXPECT_EQ(parse_op("-"), Op::Minus);
    EXPECT_EQ(parse_op("Inv"), Op::Inv);
    EXPECT_EQ(parse_op("Tr"), Op::Tr);
    EXPECT_EQ(parse_op("Gemm_NN"), Op::Gemm_NN);
    EXPECT_EQ(parse_op("Potrf_L"), Op::Potrf_L);
    EXPECT_EQ(parse_op("Trsm_LN"), Op::Trsm_LN);
}

TEST(UtilsTest, ParseOpInvalidThrows) {
    EXPECT_THROW(parse_op("NonExistentOp"), InvalidOperationError);
    EXPECT_THROW(parse_op("1234"), InvalidOperationError);
}

TEST(UtilsTest, AtomToStringConversions) {
    EXPECT_EQ(atom_to_string(Atom(Op::Add)), "+");
    EXPECT_EQ(atom_to_string(Atom(Op::Mul)), "*");
    EXPECT_EQ(atom_to_string(Atom(Op::Minus)), "-");
    EXPECT_EQ(atom_to_string(Atom(Op::Tr)), "Tr");
    EXPECT_EQ(atom_to_string(Atom(42)), "42");

    uint32_t sym_id = register_string_in_lookup("MyMatrix");
    EXPECT_EQ(atom_to_string(Atom(sym_id)), "MyMatrix");

    ScalarExpr s(2.5);
    EXPECT_EQ(atom_to_string(Atom(s)), "2.5");
}

TEST(UtilsTest, ScalarExprArithmeticAndEvaluate) {
    ScalarExpr val5(5.0);
    EXPECT_DOUBLE_EQ(val5.evaluate(), 5.0);

    ScalarExpr x('x');
    DataBindings bindings = {{"x", {3.0}}};
    EXPECT_DOUBLE_EQ(x.evaluate(bindings), 3.0);
    // Missing binding should default to 0.0
    EXPECT_DOUBLE_EQ(x.evaluate({}), 0.0);

    ScalarExpr sum = val5 + x;
    EXPECT_DOUBLE_EQ(sum.evaluate(bindings), 8.0);

    ScalarExpr diff = val5 - x;
    EXPECT_DOUBLE_EQ(diff.evaluate(bindings), 2.0);

    ScalarExpr prod = val5 * x;
    EXPECT_DOUBLE_EQ(prod.evaluate(bindings), 15.0);

    ScalarExpr quot = val5 / 2.0;
    EXPECT_DOUBLE_EQ(quot.evaluate(), 2.5);

    ScalarExpr neg = -x;
    EXPECT_DOUBLE_EQ(neg.evaluate(bindings), -3.0);
}

TEST(UtilsTest, ScalarExprComparisonAndToString) {
    ScalarExpr a(1.0);
    ScalarExpr b(2.0);
    ScalarExpr a_dup(1.0);

    EXPECT_TRUE(a == a_dup);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a < b);

    ScalarExpr expr = (ScalarExpr('k') * 2.0 + 1.0);
    EXPECT_EQ(expr.to_string(), "((k * 2) + 1)");
}

TEST(UtilsTest, SampleSizeBindingsValid) {
    std::vector<std::string> keys = {"dim1", "dim2", "dim3"};
    auto bindings = sample_size_bindings(10, 50, keys, 12345);

    EXPECT_EQ(bindings.size(), 3);
    for (const auto &key : keys) {
        ASSERT_TRUE(bindings.contains(key));
        EXPECT_GE(bindings[key], 10);
        EXPECT_LE(bindings[key], 50);
    }
}

TEST(UtilsTest, SampleSizeBindingsInvalidBoundsThrows) {
    std::vector<std::string> keys = {"N"};
    EXPECT_THROW(sample_size_bindings(100, 10, keys), std::invalid_argument);
}

TEST(UtilsTest, GenerateRandomVectorProperties) {
    auto vec = generate_random_vector(50);
    EXPECT_EQ(vec.size(), 50);
    for (double val : vec) {
        EXPECT_GE(val, -1.0);
        EXPECT_LE(val, 1.0);
    }
}

TEST(UtilsTest, GenerateIdentityMatrixProperties) {
    int n = 4;
    auto mat = generate_identity_matrix(n);
    ASSERT_EQ(mat.size(), static_cast<size_t>(n * n));

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            double expected = (r == c) ? 1.0 : 0.0;
            EXPECT_DOUBLE_EQ(mat[r * n + c], expected);
        }
    }
}

TEST(UtilsTest, IsNumericCheck) {
    EXPECT_TRUE(is_numeric({3, 4}));
    EXPECT_TRUE(is_numeric({0, 0}));
    EXPECT_FALSE(is_numeric({std::string("M"), 4}));
    EXPECT_FALSE(is_numeric({3, std::string("N")}));
    EXPECT_FALSE(is_numeric({std::string("M"), std::string("N")}));
}

TEST(UtilsTest, BindSizeAndShape) {
    SizeBindings bindings = {{"M", 10}, {"N", 20}};

    Size bound_m = bind_size(Size("M"), &bindings);
    EXPECT_TRUE(std::holds_alternative<int>(bound_m));
    EXPECT_EQ(std::get<int>(bound_m), 10);

    Size unbound_k = bind_size(Size("K"), &bindings);
    EXPECT_TRUE(std::holds_alternative<std::string>(unbound_k));
    EXPECT_EQ(std::get<std::string>(unbound_k), "K");

    Size already_int = bind_size(Size(42), &bindings);
    EXPECT_TRUE(std::holds_alternative<int>(already_int));
    EXPECT_EQ(std::get<int>(already_int), 42);

    Shape shape = {Size("M"), Size(5)};
    Shape bound_shape = bind_shape(shape, &bindings);
    EXPECT_EQ(std::get<int>(bound_shape.first), 10);
    EXPECT_EQ(std::get<int>(bound_shape.second), 5);
}

TEST(UtilsTest, MakeZeroOfShapeSquareVsNonSquare) {
    EGraph egraph;
    Id zero_sq_id = make_zero_of_shape(egraph, {3, 3});
    const auto *prop_sq = get_matrix_data(egraph, zero_sq_id);
    ASSERT_NE(prop_sq, nullptr);
    EXPECT_TRUE(prop_sq->flags.is_zero);
    EXPECT_TRUE(prop_sq->flags.is_symmetric);
    EXPECT_TRUE(prop_sq->flags.is_diagonal);

    Id zero_rect_id = make_zero_of_shape(egraph, {3, 5});
    const auto *prop_rect = get_matrix_data(egraph, zero_rect_id);
    ASSERT_NE(prop_rect, nullptr);
    EXPECT_TRUE(prop_rect->flags.is_zero);
    EXPECT_FALSE(prop_rect->flags.is_symmetric);
    EXPECT_TRUE(prop_rect->flags.is_diagonal);
}
