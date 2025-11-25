#include <gtest/gtest.h>
#include "expression.h"

TEST(Expression, ParseVariable)
{
    Expression p("x");
    EXPECT_EQ(std::get<std::string>(p.op_or_string), "x");
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
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[0].op_or_string));
    EXPECT_EQ(std::get<std::string>(p.children[0].op_or_string), "x");
    EXPECT_TRUE(std::holds_alternative<std::string>(p.children[1].op_or_string));
    EXPECT_EQ(std::get<std::string>(p.children[1].op_or_string), "y");
}

TEST(Expression, ParseNestedOperations)
{
    Expression p("Mul(Add( Add(x,y), y), Transpose(z))");
    EXPECT_EQ(std::get<Op>(p.op_or_string), Op::Mul);
    ASSERT_EQ(p.children.size(), 2);
    const Expression &add_child = p.children[0];
    EXPECT_EQ(std::get<Op>(add_child.op_or_string), Op::Add);
    ASSERT_EQ(add_child.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::string>(add_child.children[0].children[0].op_or_string));
    EXPECT_EQ(std::get<std::string>(add_child.children[0].children[0].op_or_string), "x");
    EXPECT_TRUE(std::holds_alternative<std::string>(add_child.children[1].op_or_string));
    EXPECT_EQ(std::get<std::string>(add_child.children[1].op_or_string), "y");
    const Expression &transpose_child = p.children[1];
    EXPECT_EQ(std::get<Op>(transpose_child.op_or_string), Op::Transpose);
    ASSERT_EQ(transpose_child.children.size(), 1);
    EXPECT_TRUE(std::holds_alternative<std::string>(transpose_child.children[0].op_or_string));
    EXPECT_EQ(std::get<std::string>(transpose_child.children[0].op_or_string), "z");
}