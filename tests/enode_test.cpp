#include <gtest/gtest.h>
#include "e_node.h"

using namespace std;

static ENode make_symbol(const std::string &name)
{
    return ENode(Op::Symbol, Children{}, std::string(name));
}

static ENode make_leaf(Op op)
{
    return ENode(op, Children{}, std::monostate{});
}

static ENode make_op(Op op, const Children &children)
{
    return ENode(op, children, std::monostate{});
}

TEST(ENode, IsLeafAndArity)
{
    auto s = make_symbol("x");
    EXPECT_TRUE(s.is_leaf());
    EXPECT_EQ(s.arity(), op_arity(Op::Symbol));

    auto id = make_leaf(Op::Identity);
    EXPECT_TRUE(id.is_leaf());
    EXPECT_EQ(id.arity(), op_arity(Op::Identity));

    Children children = {Id(1), Id(2)};
    auto add = make_op(Op::Add, children);
    EXPECT_FALSE(add.is_leaf());
    EXPECT_EQ(add.arity(), children.size());
}

TEST(ENode, MatchesBehavior)
{
    auto sx1 = make_symbol("x");
    auto sx2 = make_symbol("x");
    auto sy = make_symbol("y");
    EXPECT_TRUE(sx1.matches(sx2));
    EXPECT_FALSE(sx1.matches(sy));

    Children c12 = {Id(1), Id(2)};
    Children c34 = {Id(3), Id(4)};
    auto a1 = make_op(Op::Add, c12);
    auto a2 = make_op(Op::Add, c34);
    EXPECT_TRUE(a1.matches(a2));
    Children c1 = {Id(1)};
    auto a_short = make_op(Op::Add, c1);
    EXPECT_FALSE(a1.matches(a_short));
}

TEST(ENode, HashConsistencyAndSensitivity)
{
    Children c12 = {Id(1), Id(2)};
    auto n1 = make_op(Op::Mul, c12);
    auto n2 = make_op(Op::Mul, c12);
    EXPECT_EQ(n1.hash(), n2.hash());

    Children c21 = {Id(2), Id(1)};
    auto n3 = make_op(Op::Mul, c21);
    EXPECT_NE(n1.hash(), n3.hash());

    auto sx = make_symbol("x");
    auto sy = make_symbol("y");
    EXPECT_NE(sx.hash(), sy.hash());
}