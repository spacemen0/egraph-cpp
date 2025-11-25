#include <gtest/gtest.h>
#include "expression.h"

TEST(Expression, ParseVariable)
{
    Expression p("x");
    EXPECT_EQ(std::get<Expressionvar>(p.op_or_string).name, "x");
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithoutChildren)
{
    Expression p("Identity( )");
    EXPECT_EQ(std::get<Op>(p.op_or_string), Op::Identity);
    EXPECT_TRUE(p.children.empty());
}

TEST(Expression, ParseOperationWithChildren)
{
    Expression p(" Add ( x, y) ");
    EXPECT_EQ(std::get<Op>(p.op_or_string), Op::Add);
    ASSERT_EQ(p.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<Expressionvar>(p.children[0].op_or_string));
    EXPECT_EQ(std::get<Expressionvar>(p.children[0].op_or_string).name, "x");
    EXPECT_TRUE(std::holds_alternative<Expressionvar>(p.children[1].op_or_string));
    EXPECT_EQ(std::get<Expressionvar>(p.children[1].op_or_string).name, "y");
}

TEST(Expression, ParseNestedOperations)
{
    Expression p("Mul(Add( Add(x,y), y), Transpose(z))");
    EXPECT_EQ(std::get<Op>(p.op_or_string), Op::Mul);
    ASSERT_EQ(p.children.size(), 2);
    const Expression &add_child = p.children[0];
    EXPECT_EQ(std::get<Op>(add_child.op_or_string), Op::Add);
    ASSERT_EQ(add_child.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<Expressionvar>(add_child.children[0].children[0].op_or_string));
    EXPECT_EQ(std::get<Expressionvar>(add_child.children[0].children[0].op_or_string).name, "x");
    EXPECT_TRUE(std::holds_alternative<Expressionvar>(add_child.children[1].op_or_string));
    EXPECT_EQ(std::get<Expressionvar>(add_child.children[1].op_or_string).name, "y");
    const Expression &transpose_child = p.children[1];
    EXPECT_EQ(std::get<Op>(transpose_child.op_or_string), Op::Transpose);
    ASSERT_EQ(transpose_child.children.size(), 1);
    EXPECT_TRUE(std::holds_alternative<Expressionvar>(transpose_child.children[0].op_or_string));
    EXPECT_EQ(std::get<Expressionvar>(transpose_child.children[0].op_or_string).name, "z");
}