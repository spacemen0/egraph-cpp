#include <gtest/gtest.h>
#include "e_graph.h"
#include "rewrite.h"
#include "test_helper.h"

TEST(Rewrite, SimpleRewrite)
{
    EGraph egraph;
    auto a = make_symbol("a");
    Id ida = egraph.add_node(a);

    ENode zero_node({}, Atom(Op::Zero));
    Id id0 = egraph.add_node(zero_node);

    ENode mul_node({ida, id0}, Atom(Op::Mul));
    Id id_mul = egraph.add_node(mul_node);

    // Rule: x * 0 -> 0
    Pattern lhs = {
        PatternAtom(Op::Mul),
        {Pattern{PatternAtom(PatternVar{"x"}), {}},
         Pattern{PatternAtom(Op::Zero), {}}}};
    Pattern rhs = {
        PatternAtom(Op::Zero),
        {}};

    Rewrite rule{"mul_zero", lhs, rhs};

    EXPECT_NE(id_mul, id0);

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity)
{
    EGraph egraph;

    auto a = make_symbol("a");
    auto b = make_symbol("b");
    Id ida = egraph.add_node(a);
    Id idb = egraph.add_node(b);

    ENode add_node({ida, idb}, Atom(Op::Add));
    Id id_add = egraph.add_node(add_node);

    // Rule: x + y -> y + x
    Pattern lhs = {
        PatternAtom(Op::Add),
        {Pattern{PatternAtom(PatternVar{"x"}), {}},
         Pattern{PatternAtom(PatternVar{"y"}), {}}}};
    Pattern rhs = {
        PatternAtom(Op::Add),
        {Pattern{PatternAtom(PatternVar{"y"}), {}},
         Pattern{PatternAtom(PatternVar{"x"}), {}}}};

    Rewrite rule{"commute_add", lhs, rhs};

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);

    ENode commuted_node({idb, ida}, Atom(Op::Add));
    Id id_commuted = egraph.add_node(commuted_node);

    EXPECT_EQ(id_add, id_commuted);
}

TEST(Rewrite, NoMatch)
{
    EGraph egraph;

    auto a = make_symbol("a");
    Id ida = egraph.add_node(a);

    // Rule: x + 0 -> x
    Pattern lhs = {
        PatternAtom(Op::Add),
        {Pattern{PatternAtom(PatternVar{"x"}), {}},
         Pattern{PatternAtom(Op::Zero), {}}}};
    Pattern rhs = {
        PatternAtom(PatternVar{"x"}),
        {}};

    Rewrite rule{"add_zero", lhs, rhs};

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_FALSE(changed);
}

TEST(Rewrite, NewNodes)
{
    EGraph egraph;

    auto x = make_symbol("x");
    Id idx = egraph.add_node(x);

    // Rule: x -> x + 0
    Pattern lhs = {
        PatternAtom(PatternVar{"x"}),
        {}};
    Pattern rhs = {
        PatternAtom(Op::Add),
        {Pattern{PatternAtom(PatternVar{"x"}), {}},
         Pattern{PatternAtom(Op::Zero), {}}}};

    Rewrite rule{"add_zero_rhs", lhs, rhs};

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);

    ENode add_node({idx, egraph.add_node(ENode({}, Atom(Op::Zero)))}, Atom(Op::Add));
    Id id_add = egraph.add_node(add_node);

    EXPECT_EQ(egraph.find_class_id(idx), egraph.find_class_id(id_add));
}