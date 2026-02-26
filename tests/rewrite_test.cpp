#include <gtest/gtest.h>
#include "e_graph.h"
#include "rewriter.h"
#include "test_helper.h"
#include "rewrite_rules.h"

TEST(Rewrite, SimpleRewrite)
{
    EGraph egraph(get_property_table());

    ENode zero_node({}, "Zero");
    Id id0 = egraph.add_node(zero_node);

    Id id_mul = egraph.add_expression(Expression("Mul(A, Zero)"));
    // egraph.print_egraph();

    // x * 0 -> 0
    Pattern lhs("Mul(?x, ?z)");
    Pattern rhs("?z");

    EXPECT_NE(id_mul, id0);
    std::vector<Rewrite> rules = {
        make_rewrite("mul_zero", "Mul(?x, ?z)", "?z", [](const EGraph &g, const Substitution &s)
                     { return is_zero(s, g, "z"); }, nullptr)};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity)
{
    EGraph egraph(get_property_table());

    Id id_add = egraph.add_expression(Expression("Add(A, Z)"));

    // x + y -> y + x
    Pattern lhs("Add(?x, ?y)");
    Pattern rhs("Add(?y, ?x)");
    Rewrite rule{"commute_add", lhs, rhs};

    EXPECT_NE(id_add, egraph.add_expression(Expression("Add(Z, A)")));

    std::vector<Rewrite> rules = {{"commute_add", lhs, rhs}};
    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_commuted = egraph.add_expression(Expression("Add(Z, A)"));

    EXPECT_EQ(egraph.find_class_id(id_add), egraph.find_class_id(id_commuted));
}

TEST(Rewrite, NoMatch)
{
    EGraph egraph(get_property_table());

    egraph.add_node(sym_a);

    // x + 0 -> x
    Pattern lhs("Add(?x, Zero)");
    Pattern rhs("?x");

    std::vector<Rewrite> rules = {{"add_zero", lhs, rhs}};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_FALSE(changed);
}

TEST(Rewrite, NewNodes)
{
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {10, 10};
    pt.add_property_entry("a", prop_a);
    EGraph egraph(std::move(pt));

    Id id_add = egraph.add_expression(Expression("Mul(Inv(a), a)"));

    std::vector<Rewrite> rules = {
        make_rewrite("inv-mul-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s)
                     { return make_identity_for(g, s, "a"); })};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    // Check the new identity node
    auto results = egraph.find_node_id(ENode({}, "I_10x10"));
    EXPECT_TRUE(results.has_value());
    EXPECT_EQ(results.value(), egraph.find_class_id(id_add));
}

TEST(Rewrite, SolveRule)
{
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {3, 3};
    pt.add_property_entry("a", prop_a);

    MatrixProperty prop_b;
    prop_b.shape = {3, 2};
    pt.add_property_entry("b", prop_b);

    EGraph egraph(std::move(pt));

    Id id_expr = egraph.add_expression(Expression("Mul(Inv(a), b)"));

    Rewriter rewriter(egraph, {solve_rule}, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_solve = egraph.add_expression(Expression("Sol(a, b)"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_solve));
    EXPECT_EQ(std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape, std::make_pair(Size(3), Size(2)));
}