#include <gtest/gtest.h>
#include "e_graph.h"
#include "rewriter.h"
#include "test_helper.h"

TEST(Rewrite, SimpleRewrite)
{
    EGraph egraph(get_property_table());
    ENode zero_node({}, Atom(Op::Zero));
    Id id0 = egraph.add_node(zero_node);

    Id id_mul = egraph.add_expression(Expression("Mul(A, Zero)"));
    egraph.print_egraph();
    // Rule: x * 0 -> 0
    Pattern lhs("Mul(?x, Zero)");
    Pattern rhs("Zero");

    EXPECT_NE(id_mul, id0);
    std::vector<Rewrite> rules = {{"mul_zero", lhs, rhs}};
    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity)
{
    EGraph egraph(get_property_table());

    Id id_add = egraph.add_expression(Expression("Add(a, b)"));

    // Rule: x + y -> y + x
    Pattern lhs("Add(?x, ?y)");
    Pattern rhs("Add(?y, ?x)");
    Rewrite rule{"commute_add", lhs, rhs};

    EXPECT_NE(id_add, egraph.add_expression(Expression("Add(b, a)")));

    std::vector<Rewrite> rules = {{"commute_add", lhs, rhs}};
    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_commuted = egraph.add_expression(Expression("Add(b, a)"));

    EXPECT_EQ(egraph.find_class_id(id_add), egraph.find_class_id(id_commuted));
}

TEST(Rewrite, NoMatch)
{
    EGraph egraph(get_property_table());

    egraph.add_node(sym_a);

    // Rule: x + 0 -> x
    Pattern lhs("Add(?x, Zero)");
    Pattern rhs("?x");

    std::vector<Rewrite> rules = {{"add_zero", lhs, rhs}};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_FALSE(changed);
}

TEST(Rewrite, NewNodes)
{
    EGraph egraph(get_property_table());

    Id id_add = egraph.add_expression(Expression("Mul(Invert(a), a)"));

    // Rule: (* (invert a) a) -> "Identity"
    Pattern lhs("Mul(Invert(?a), ?a)");
    Pattern rhs("Identity");

    std::vector<Rewrite> rules = {{"mul_invert_identity", lhs, rhs}};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    EXPECT_EQ(egraph.find(ENode({}, Op::Identity)).value(), egraph.find_class_id(id_add));
}