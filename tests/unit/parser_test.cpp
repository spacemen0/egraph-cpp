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
    // The second child should be B * C, serialized to prefix Mul(B, C)
    EXPECT_EQ(parsed.children_strings[1], "Mul(B, C)");
}

TEST(ParserTest, InfixParentheses) {
    auto parsed = parser::parse_expression("(A + B) * C");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Mul);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    // The first child should be A + B, serialized to prefix Add(A, B)
    EXPECT_EQ(parsed.children_strings[0], "Add(A, B)");
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
    EXPECT_TRUE(std::holds_alternative<int>(parsed.atom));
    EXPECT_EQ(std::get<int>(parsed.atom), -42);
    EXPECT_TRUE(parsed.children_strings.empty());
}

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
    EXPECT_EQ(parsed.children_strings[0], "Mul(A, B)");
}

TEST(ParserTest, OldPrefixSyntax) {
    auto parsed = parser::parse_expression("Add(Mul(A, B), C)");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Add);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "Mul(A, B)");
    EXPECT_EQ(parsed.children_strings[1], "C");
}

TEST(ParserTest, ComplexMixed) {
    auto parsed = parser::parse_expression("Inv(A + B) * Tr(C)");
    EXPECT_TRUE(std::holds_alternative<Op>(parsed.atom));
    EXPECT_EQ(std::get<Op>(parsed.atom), Op::Mul);
    ASSERT_EQ(parsed.children_strings.size(), 2);
    EXPECT_EQ(parsed.children_strings[0], "Inv(Add(A, B))");
    EXPECT_EQ(parsed.children_strings[1], "Tr(C)");
}
