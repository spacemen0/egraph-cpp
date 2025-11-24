#include <gtest/gtest.h>
#include "pattern.h"

TEST(Pattern, ParseVariable)
{
    Pattern p("x");
    EXPECT_EQ(std::get<PatternVar>(p.op_or_var).name, "x");
    EXPECT_TRUE(p.children.empty());
}

TEST(Pattern, ParseOperationWithoutChildren)
{
    Pattern p("Identity( )");
    EXPECT_EQ(std::get<Op>(p.op_or_var), Op::Identity);
    EXPECT_TRUE(p.children.empty());
}

TEST(Pattern, ParseOperationWithChildren)
{
    Pattern p(" Add ( x, y) ");
    EXPECT_EQ(std::get<Op>(p.op_or_var), Op::Add);
    ASSERT_EQ(p.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<PatternVar>(p.children[0].op_or_var));
    EXPECT_EQ(std::get<PatternVar>(p.children[0].op_or_var).name, "x");
    EXPECT_TRUE(std::holds_alternative<PatternVar>(p.children[1].op_or_var));
    EXPECT_EQ(std::get<PatternVar>(p.children[1].op_or_var).name, "y");
}

TEST(Pattern, ParseNestedOperations)
{
    Pattern p("Mul(Add( Add(x,y), y), Transpose(z))");
    EXPECT_EQ(std::get<Op>(p.op_or_var), Op::Mul);
    ASSERT_EQ(p.children.size(), 2);
    const Pattern &add_child = p.children[0];
    EXPECT_EQ(std::get<Op>(add_child.op_or_var), Op::Add);
    ASSERT_EQ(add_child.children.size(), 2);
    EXPECT_TRUE(std::holds_alternative<PatternVar>(add_child.children[0].children[0].op_or_var));
    EXPECT_EQ(std::get<PatternVar>(add_child.children[0].children[0].op_or_var).name, "x");
    EXPECT_TRUE(std::holds_alternative<PatternVar>(add_child.children[1].op_or_var));
    EXPECT_EQ(std::get<PatternVar>(add_child.children[1].op_or_var).name, "y");
    const Pattern &transpose_child = p.children[1];
    EXPECT_EQ(std::get<Op>(transpose_child.op_or_var), Op::Transpose);
    ASSERT_EQ(transpose_child.children.size(), 1);
    EXPECT_TRUE(std::holds_alternative<PatternVar>(transpose_child.children[0].op_or_var));
    EXPECT_EQ(std::get<PatternVar>(transpose_child.children[0].op_or_var).name, "z");
}