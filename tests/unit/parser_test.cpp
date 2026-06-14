#include "errors.h"
#include "parser.h"
#include <gtest/gtest.h>

TEST(ParserTest, InfixBasic) {
    auto parsed = parser::parse_expression("A + B");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Add);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "A");
    EXPECT_EQ(parsed.children_strings[1], "B");
}

TEST(ParserTest, InfixPrecedence) {
    auto parsed = parser::parse_expression("A + B * C");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Add);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "A");
    // The second child should be B * C
    EXPECT_EQ(parsed.children_strings[1], "B * C");
}

TEST(ParserTest, InfixParentheses) {
    auto parsed = parser::parse_expression("(A + B) * C");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Mul);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    // The first child should be A + B
    EXPECT_EQ(parsed.children_strings[0], "A + B");
    EXPECT_EQ(parsed.children_strings[1], "C");
}

TEST(ParserTest, UnaryMinus) {
    auto parsed = parser::parse_expression("-X");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Minus);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "0");
    EXPECT_EQ(parsed.children_strings[1], "X");
}

TEST(ParserTest, NegativeInteger) {
    auto parsed = parser::parse_expression("-42");
    EXPECT_TRUE(std::holds_alternative<double>(parsed.atom));
    EXPECT_EQ(std::get<double>(parsed.atom), -42.0);
    EXPECT_TRUE(parsed.children_strings.empty());
}

TEST(ParserTest, DecimalNumber) {
    auto parsed = parser::parse_expression("3.14");
    EXPECT_TRUE(std::holds_alternative<double>(parsed.atom));
    EXPECT_DOUBLE_EQ(std::get<double>(parsed.atom), 3.14);
    EXPECT_TRUE(parsed.children_strings.empty());
}

TEST(ParserTest, LeadingDecimal) {
    auto parsed = parser::parse_expression(".5");
    EXPECT_TRUE(std::holds_alternative<double>(parsed.atom));
    EXPECT_DOUBLE_EQ(std::get<double>(parsed.atom), 0.5);
}

TEST(ParserTest, TrailingDecimal) {
    auto parsed = parser::parse_expression("42.");
    EXPECT_TRUE(std::holds_alternative<double>(parsed.atom));
    EXPECT_DOUBLE_EQ(std::get<double>(parsed.atom), 42.0);
}

TEST(ParserTest, NegativeDecimal) {
    auto parsed = parser::parse_expression("-0.001");
    EXPECT_TRUE(std::holds_alternative<double>(parsed.atom));
    EXPECT_DOUBLE_EQ(std::get<double>(parsed.atom), -0.001);
}

TEST(ParserTest, InvalidNumber) { EXPECT_THROW(parser::parse_expression("3.14.15"), ParseError); }

TEST(ParserTest, FunctionCall) {
    auto parsed = parser::parse_expression("Inv(A)");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Inv);
    ASSERT_EQ(parsed.children_strings.size(), 1);
    EXPECT_EQ(parsed.children_strings[0], "A");
}

TEST(ParserTest, FunctionCallWithInfix) {
    auto parsed = parser::parse_expression("Tr(A * B)");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Tr);
    ASSERT_EQ(parsed.children_strings.size(), 1);
    EXPECT_EQ(parsed.children_strings[0], "A * B");
}

TEST(ParserTest, DisallowOldPrefixSyntax) {
    EXPECT_THROW(parser::parse_expression("Add(A, B)"), ParseError);
    EXPECT_THROW(parser::parse_expression("Mul(A, B)"), ParseError);
    EXPECT_THROW(parser::parse_expression("Minus(A, B)"), ParseError);
}

TEST(ParserTest, ComplexMixed) {
    auto parsed = parser::parse_expression("Inv(A + B) * Tr(C)");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Mul);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "Inv(A + B)");
    EXPECT_EQ(parsed.children_strings[1], "Tr(C)");
}

TEST(ParserTest, BlasLapackKernels) {
    auto gemm = parser::parse_expression("Gemm_NN(1, 0, 0, 0, A, B, C)");
    EXPECT_EQ(std::get<Op>(gemm.atom), Op::Gemm_NN);
    ASSERT_EQ(gemm.children_strings.size(), 7);
    EXPECT_EQ(gemm.children_strings[4], "A");
    EXPECT_EQ(gemm.children_strings[5], "B");
    EXPECT_EQ(gemm.children_strings[6], "C");

    auto potrf = parser::parse_expression("Potrf_L(0, A)");
    EXPECT_EQ(std::get<Op>(potrf.atom), Op::Potrf_L);
    ASSERT_EQ(potrf.children_strings.size(), 2);
    EXPECT_EQ(potrf.children_strings[1], "A");

    auto gemv = parser::parse_expression("Gemv_N(1, 0, 0, A, x, y)");
    EXPECT_EQ(std::get<Op>(gemv.atom), Op::Gemv_N);
    ASSERT_EQ(gemv.children_strings.size(), 6);
    EXPECT_EQ(gemv.children_strings[3], "A");

    auto syrk = parser::parse_expression("Syrk_N(1, 0, 0, 0, A, C)");
    EXPECT_EQ(std::get<Op>(syrk.atom), Op::Syrk_N);
    ASSERT_EQ(syrk.children_strings.size(), 6);
    EXPECT_EQ(syrk.children_strings[4], "A");
    EXPECT_EQ(syrk.children_strings[5], "C");

    auto trsm = parser::parse_expression("Trsm_LN(1, 0, 0, 0, 0, A, B)");
    EXPECT_EQ(std::get<Op>(trsm.atom), Op::Trsm_LN);
    ASSERT_EQ(trsm.children_strings.size(), 7);
    EXPECT_EQ(trsm.children_strings[5], "A");
    EXPECT_EQ(trsm.children_strings[6], "B");
}
