#include "e_graph.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST(Rewrite, SimpleRewrite) {
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
        make_rewrite("mul_zero", "Mul(?x, ?z)", "?z", [](const EGraph &g, const Substitution &s) {
        return is_zero(s, g, "z");
    }, nullptr)};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity) {
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

TEST(Rewrite, NoMatch) {
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

TEST(Rewrite, NewNodes) {
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {10, 10};
    pt.add_or_update_property_entry("a", prop_a);
    EGraph egraph(std::move(pt));

    Id id_add = egraph.add_expression(Expression("Mul(Inv(a), a)"));

    std::vector<Rewrite> rules = {
        make_rewrite("inv-mul-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
        return make_identity_for(g, s, "a");
    })};

    Rewriter rewriter(egraph, rules, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    // Check the new identity node
    auto results = egraph.find_node_id(ENode({}, "I_10x10"));
    EXPECT_TRUE(results.has_value());
    EXPECT_EQ(results.value(), egraph.find_class_id(id_add));
}

TEST(Rewrite, SolveRule) {
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {3, 3};
    pt.add_or_update_property_entry("a", prop_a);

    MatrixProperty prop_b;
    prop_b.shape = {3, 2};
    pt.add_or_update_property_entry("b", prop_b);

    EGraph egraph(std::move(pt));

    Id id_expr = egraph.add_expression(Expression("Mul(Inv(a), b)"));

    Rewriter rewriter(egraph, {solver_left}, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_solve = egraph.add_expression(Expression("Sol(a, b)"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_solve));
    EXPECT_EQ(
        std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape,
        std::make_pair(Size(3), Size(2)));
}

TEST(Rewrite, LLtRewrite) {
    EGraph egraph(get_property_table());

    Id id_expr = egraph.add_expression(Expression("Inv(V)"));

    Rewriter rewriter(egraph, {llt_invert}, 100);
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_llt = egraph.add_expression(Expression("Mul(Tr(Inv(Get(LLt(V), 0))), Inv(Get(LLt(V), 0)))"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_llt));
}

TEST(Rewrite, BackoffScheduler) {
    PropertyTable pt;

    MatrixProperty prop_3x3;
    prop_3x3.shape = {3, 3};
    pt.add_or_update_property_entry("a", prop_3x3);

    MatrixProperty prop_4x4;
    prop_4x4.shape = {4, 4};
    pt.add_or_update_property_entry("b", prop_4x4);

    EGraph egraph(std::move(pt));

    Id id1 = egraph.add_expression(Expression("Inv(Inv(Inv(a)))"));
    Id id2 = egraph.add_expression(Expression("Inv(Inv(Inv(b)))"));

    std::vector<Rewrite> rules = {make_rewrite("inv_inv", "Inv(Inv(?x))", "?x", nullptr, nullptr, 1)};

    Rewriter rewriter(egraph, rules, 1000, true);

    bool changed1 = rewriter.apply_one_iteration();
    EXPECT_TRUE(changed1);

    bool changed2 = rewriter.apply_one_iteration();
    EXPECT_FALSE(changed2);

    bool changed3 = rewriter.apply_one_iteration();
    EXPECT_TRUE(changed3);
}