#include <gtest/gtest.h>
#include "expression.h"

TEST(Expression, ParseVariable)
{
    Expression p("X");
    EXPECT_EQ(std::get<std::string>(p.atom), "X");
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithoutChildren)
{
    Expression p("Identity");
    EXPECT_EQ(std::get<std::string>(p.atom), "Identity");
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithChildren)
{
    Expression p(" Add ( X, Y) ");
    EXPECT_EQ(std::get<Op>(p.atom), Op::Add);
    ASSERT_EQ(p.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[0].atom));
    EXPECT_EQ(std::get<std::string>(p.children[0].atom), "X");
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[1].atom));
    EXPECT_EQ(std::get<std::string>(p.children[1].atom), "Y");
}

TEST(Expression, ParseNestedOperations)
{
    Expression p("Mul(Add( Add(X,Y), Y), Transpose(Z))");
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
    EXPECT_EQ(std::get<Op>(transpose_child.atom), Op::Transpose);
    ASSERT_EQ(transpose_child.children.size(), 1);
    EXPECT_TRUE(std::holds_alternative<std::string>(transpose_child.children[0].atom));
    EXPECT_EQ(std::get<std::string>(transpose_child.children[0].atom), "Z");
}