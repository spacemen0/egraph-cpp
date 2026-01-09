#include <gtest/gtest.h>
#include "e_graph.h"
#include "rewrite.h"
#include "test_helper.h"

const ENode sym_a = make_symbol("a");

TEST(Rewrite, SimpleRewrite)
{
    EGraph egraph;
    ENode zero_node({}, Atom(Op::Zero));
    Id id0 = egraph.add_node(zero_node);

    Id id_mul = egraph.add_expression(Expression("Mul(A, Zero)"));
    egraph.print_egraph();
    // Rule: x * 0 -> 0
    Pattern lhs("Mul(?x, Zero)");
    Pattern rhs("Zero");

    Rewrite rule{"mul_zero", lhs, rhs};

    EXPECT_NE(id_mul, id0);

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity)
{
    EGraph egraph;

    Id id_add = egraph.add_expression(Expression("Add(a, b)"));

    // Rule: x + y -> y + x
    Pattern lhs("Add(?x, ?y)");
    Pattern rhs("Add(?y, ?x)");
    Rewrite rule{"commute_add", lhs, rhs};

    EXPECT_NE(id_add, egraph.add_expression(Expression("Add(b, a)")));

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);

    Id id_commuted = egraph.add_expression(Expression("Add(b, a)"));

    EXPECT_EQ(egraph.find_class_id(id_add), egraph.find_class_id(id_commuted));
}

TEST(Rewrite, NoMatch)
{
    EGraph egraph;

    egraph.add_node(sym_a);

    // Rule: x + 0 -> x
    Pattern lhs("Add(?x, Zero)");
    Pattern rhs("?x");

    Rewrite rule{"add_zero", lhs, rhs};

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_FALSE(changed);
}

TEST(Rewrite, NewNodes)
{
    EGraph egraph;

    Id id_add = egraph.add_expression(Expression("Mul(Invert(a), a)"));

    // Rule: (* (invert a) a) -> "Identity"
    Pattern lhs("Mul(Invert(?a), ?a)");
    Pattern rhs("Identity");

    Rewrite rule{"mul_invert_identity", lhs, rhs};

    bool changed = apply_rewrites(egraph, {rule});
    EXPECT_TRUE(changed);

    EXPECT_EQ(egraph.find(ENode({}, Op::Identity)).value(), egraph.find_class_id(id_add));
}