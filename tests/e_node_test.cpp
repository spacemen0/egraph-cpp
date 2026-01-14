#include <gtest/gtest.h>
#include "e_node.h"
#include "test_helper.h"

using enum Op;

TEST(ENode, IsLeaf)
{
    auto s = make_symbol("S");
    EXPECT_TRUE(s.is_leaf());

    Children children = {Id(1), Id(2)};
    auto add = make_op(Add, children);
    EXPECT_FALSE(add.is_leaf());
}

TEST(ENode, MatchesBehavior)
{
    auto sx1 = make_symbol("X");
    auto sx2 = make_symbol("X");
    auto sy = make_symbol("Y");
    EXPECT_TRUE(sx1.equals(sx2));
    EXPECT_FALSE(sx1.equals(sy));

    Children c12 = {Id(1), Id(2)};
    Children c34 = {Id(3), Id(4)};
    auto a1 = make_op(Add, c12);
    auto a2 = make_op(Add, c34);
    EXPECT_FALSE(a1.equals(a2));
    Children c1 = {Id(1)};
    auto a_short = make_op(Add, c1);
    EXPECT_FALSE(a1.equals(a_short));
}

TEST(ENode, HashConsistencyAndSensitivity)
{
    Children c12 = {Id(1), Id(2)};
    auto n1 = make_op(Mul, c12);
    auto n2 = make_op(Mul, c12);
    EXPECT_EQ(n1.hash(), n2.hash());

    Children c21 = {Id(2), Id(1)};
    auto n3 = make_op(Mul, c21);
    EXPECT_NE(n1.hash(), n3.hash());

    auto sx = make_symbol("X");
    auto sy = make_symbol("Y");
    EXPECT_NE(sx.hash(), sy.hash());
}