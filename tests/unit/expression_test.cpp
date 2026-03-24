#include "errors.h"
#include "expression.h"
#include <gtest/gtest.h>

TEST(Expression, ParseVariable) {
    Expression p("X");
    EXPECT_EQ(std::get<std::string>(p.atom), "X");
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithoutChildren) {
    Expression p("Identity");
    EXPECT_EQ(std::get<std::string>(p.atom), "Identity");
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithChildren) {
    Expression p(" Add ( X, Y) ");
    EXPECT_EQ(std::get<Op>(p.atom), Op::Add);
    ASSERT_EQ(p.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[0].atom));
    EXPECT_EQ(std::get<std::string>(p.children[0].atom), "X");
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[1].atom));
    EXPECT_EQ(std::get<std::string>(p.children[1].atom), "Y");
}

TEST(Expression, ParseNestedOperations) {
    Expression p("Mul(Add( Add(X,Y), Y), Tr(Z))");
    EXPECT_EQ(std::get<Op>(p.atom), Op::Mul);
    ASSERT_EQ(p.children.size(), 2);
    const Expression &add_child = p.children[0];
    EXPECT_EQ(std::get<Op>(add_child.atom), Op::Add);
    ASSERT_EQ(add_child.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::string>(add_child.children[0].children[0].atom));
    EXPECT_EQ(std::get<std::string>(add_child.children[0].children[0].atom), "X");
    EXPECT_TRUE(std::holds_alternative<std::string>(add_child.children[1].atom));
    EXPECT_EQ(std::get<std::string>(add_child.children[1].atom), "Y");
    const Expression &transpose_child = p.children[1];
    EXPECT_EQ(std::get<Op>(transpose_child.atom), Op::Tr);
    ASSERT_EQ(transpose_child.children.size(), 1);
    EXPECT_TRUE(std::holds_alternative<std::string>(transpose_child.children[0].atom));
    EXPECT_EQ(std::get<std::string>(transpose_child.children[0].atom), "Z");
}

TEST(Expression, ToString) {
    Expression p("Mul(Add(X, Y), Tr(Z))");
    std::string repr = p.to_string();
    EXPECT_EQ(repr, "Mul(Add(X, Y), Tr(Z))");
}

TEST(Expression, ToHumanString) {
    Expression p("Mul(Add(X, Y), Tr(Z))");
    std::string repr = p.to_human_string();
    EXPECT_EQ(repr, "(X + Y) * Zᵀ");
}

TEST(Expression, ToHumanStringFactorizationIndexing) {
    Expression q("Get(QR(A), 0)");
    Expression r("Get(QR(A), 1)");
    EXPECT_EQ(q.to_human_string(), "Q(A)");
    EXPECT_EQ(r.to_human_string(), "R(A)");
}

TEST(Expression, ToHumanStringMulChainPreservesLeftGrouping) {
    Expression p("Mul(Mul(A, B), C)");
    EXPECT_EQ(p.to_human_string(), "(A * B) * C");
}

TEST(Expression, ToHumanStringMulChainPreservesRightGrouping) {
    Expression p("Mul(A, Mul(B, C))");
    EXPECT_EQ(p.to_human_string(), "A * (B * C)");
}

TEST(Expression, ParseQRNode) {
    Expression p("Mul(Add(Get(QR(A), 0)), Get(QR(A), 1))");
    EXPECT_EQ(std::get<Op>(p.atom), Op::Mul);
    ASSERT_EQ(p.children.size(), 2);
    const Expression &add_child = p.children[0];
    EXPECT_EQ(std::get<Op>(add_child.atom), Op::Add);
    ASSERT_EQ(add_child.children.size(), 1);
    const Expression &get_q = add_child.children[0];
    EXPECT_EQ(std::get<Op>(get_q.atom), Op::Get);
    ASSERT_EQ(get_q.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<Op>(get_q.children[0].atom));
    EXPECT_EQ(std::get<Op>(get_q.children[0].atom), Op::QR);
    EXPECT_TRUE(std::holds_alternative<int>(get_q.children[1].atom));
    EXPECT_EQ(std::get<int>(get_q.children[1].atom), 0);
    const Expression &get_r = p.children[1];
    EXPECT_EQ(std::get<Op>(get_r.atom), Op::Get);
    ASSERT_EQ(get_r.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<Op>(get_r.children[0].atom));
    EXPECT_EQ(std::get<Op>(get_r.children[0].atom), Op::QR);
    EXPECT_TRUE(std::holds_alternative<int>(get_r.children[1].atom));
    EXPECT_EQ(std::get<int>(get_r.children[1].atom), 1);
}

TEST(Expression, ParsingErrors) {
    EXPECT_THROW(Expression(""), ParseError);
    EXPECT_THROW(Expression("Add(X, Y"), ParseError);
}
