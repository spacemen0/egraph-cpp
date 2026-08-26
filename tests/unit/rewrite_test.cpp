#include "e_graph.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
using namespace egraph;



TEST(Rewrite, SimpleRewrite) {
    EGraph egraph(get_property_table());

    ENode zero_node({}, register_string_in_lookup("Zero"));
    Id id0 = egraph.add_node(zero_node);

    Id id_mul = egraph.add_expression(Expression("A * Zero"));
    // egraph.print_egraph();

    // x * 0 -> 0
    Pattern lhs("?x * ?z");
    Pattern rhs("?z");

    EXPECT_NE(id_mul, id0);
    std::vector<Rewrite> rules = {make_rewrite("mul_zero", "?x * ?z", "?z", false, is_zero_cond("z"), nullptr)};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);
    EXPECT_EQ(egraph.find_class_id(id_mul), egraph.find_class_id(id0));
}

TEST(Rewrite, Commutativity) {
    EGraph egraph(get_property_table());

    Id id_add = egraph.add_expression(Expression("A + Z"));

    // x + y -> y + x
    Pattern lhs("?x + ?y");
    Pattern rhs("?y + ?x");
    Rewrite rule{"commute_add", lhs, rhs};

    EXPECT_NE(id_add, egraph.add_expression(Expression("Z + A")));

    std::vector<Rewrite> rules = {{"commute_add", lhs, rhs}};
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_commuted = egraph.add_expression(Expression("Z + A"));

    EXPECT_EQ(egraph.find_class_id(id_add), egraph.find_class_id(id_commuted));
}

TEST(Rewrite, NoMatch) {
    EGraph egraph(get_property_table());

    egraph.add_node(sym_a);

    // x + 0 -> x
    Pattern lhs("?x + Zero");
    Pattern rhs("?x");

    std::vector<Rewrite> rules = {{"add_zero", lhs, rhs}};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_FALSE(changed);
}

TEST(Rewrite, NewNodes) {
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {10, 10};
    prop_a.flags.is_non_singular = true;
    pt.add_or_update_property_entry("a", prop_a);
    EGraph egraph(std::move(pt));

    Id id_add = egraph.add_expression(Expression("Inv(a) * a"));

    std::vector<Rewrite> rules = {make_rewrite(
        "inv-mul-left", "Inv(?a) * ?a", "?__dynamic__", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
        return std::make_pair(make_identity_for(g, s, "a"), false);
    })};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    // Check the new identity node
    auto results = egraph.find_node_id(ENode({}, register_string_in_lookup("I_10x10")));
    EXPECT_TRUE(results.has_value());
    EXPECT_EQ(results.value(), egraph.find_class_id(id_add));
}

TEST(Rewrite, SolveRule) {
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {3, 3};
    prop_a.flags.is_non_singular = true;
    pt.add_or_update_property_entry("a", prop_a);

    MatrixProperty prop_b;
    prop_b.shape = {3, 2};
    prop_b.flags.is_non_singular = true;
    pt.add_or_update_property_entry("b", prop_b);

    EGraph egraph(std::move(pt));

    Id id_expr = egraph.add_expression(Expression("Inv(a) * b"));

    Rewriter rewriter(egraph, {solver_left}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_solve = egraph.add_expression(Expression("Sol(a, b)"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_solve));
    EXPECT_EQ(
        std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape,
        std::make_pair(Size(3), Size(2)));
}

TEST(Rewrite, SolR_RightSolve) {
    PropertyTable pt;

    MatrixProperty prop_a;
    prop_a.shape = {3, 3};
    prop_a.flags.is_non_singular = true;
    prop_a.flags.is_lower_triangular = true;
    pt.add_or_update_property_entry("a", prop_a);

    MatrixProperty prop_b;
    prop_b.shape = {2, 3};
    prop_b.flags.is_non_singular = true;
    pt.add_or_update_property_entry("b", prop_b);

    EGraph egraph(std::move(pt));

    Id id_expr = egraph.add_expression(Expression("b * Inv(a)"));

    Rewriter rewriter(egraph, {solver_right, trsm_rn}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_solr = egraph.add_expression(Expression("SolR(a, b)"));
    Id id_trsm_rn = egraph.add_expression(Expression("Trsm_RN(a, b)"));

    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_solr));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_trsm_rn));
    EXPECT_EQ(
        std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape,
        std::make_pair(Size(2), Size(3)));
}

TEST(Rewrite, LLtRewrite) {
    EGraph egraph(get_property_table());

    Id id_expr = egraph.add_expression(Expression("Inv(V)"));

    Rewriter rewriter(egraph, {llt_invert}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_llt = egraph.add_expression(Expression("Tr(Inv(Get(LLt(V), 0))) * Inv(Get(LLt(V), 0))"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_llt));
}

TEST(Rewrite, LLtToUtURewrite) {
    EGraph egraph(get_property_table());

    Id id_llt = egraph.add_expression(Expression("Get(LLt(V), 0)"));

    Rewriter rewriter(egraph, {llt_to_utu}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_utu = egraph.add_expression(Expression("Tr(Get(UtU(V), 0))"));
    EXPECT_EQ(egraph.find_class_id(id_llt), egraph.find_class_id(id_utu));
}

TEST(Rewrite, BackoffScheduler) {
    PropertyTable pt;

    MatrixProperty prop_3x3;
    prop_3x3.shape = {3, 3};
    prop_3x3.flags.is_non_singular = true;
    pt.add_or_update_property_entry("a", prop_3x3);

    MatrixProperty prop_4x4;
    prop_4x4.shape = {4, 4};
    prop_4x4.flags.is_non_singular = true;
    pt.add_or_update_property_entry("b", prop_4x4);

    EGraph egraph(std::move(pt));

    egraph.add_expression(Expression("Inv(Inv(Inv(a)))"));
    egraph.add_expression(Expression("Inv(Inv(Inv(b)))"));

    std::vector<Rewrite> rules = {make_rewrite("inv_inv", "Inv(Inv(?x))", "?x", false, nullptr, nullptr, 2)};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000, .enable_backoff = true}});

    bool changed1 = rewriter.apply_one_iteration();
    EXPECT_TRUE(changed1);

    bool changed2 = rewriter.apply_one_iteration();
    EXPECT_FALSE(changed2);

    bool changed3 = rewriter.apply_one_iteration();
    EXPECT_TRUE(changed3);
}

TEST(Rewrite, LowerAxpy) {
    PropertyTable pt;
    MatrixProperty vec_prop;
    vec_prop.shape = {3, 1};
    pt.add_or_update_property_entry("x", vec_prop);
    pt.add_or_update_property_entry("y", vec_prop);

    MatrixProperty mat_prop;
    mat_prop.shape = {2, 2};
    pt.add_or_update_property_entry("A", mat_prop);
    pt.add_or_update_property_entry("B", mat_prop);

    EGraph egraph(std::move(pt));
    Id vec_add_id = egraph.add_expression(Expression("x + y"));
    Id mat_add_id = egraph.add_expression(Expression("A + B"));

    std::vector<Rewrite> rules = build_rewrite_sets({"lowering"});
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 100}});
    rewriter.apply_rewrites(5);

    // Verify Axpy expressions exist for both vector and matrix addition
    Id vec_axpy_id = egraph.add_expression(Expression("Axpy(x, y)"));
    Id mat_axpy_id = egraph.add_expression(Expression("Axpy(A, B)"));

    EXPECT_EQ(egraph.find_class_id(vec_add_id), egraph.find_class_id(vec_axpy_id));
    EXPECT_EQ(egraph.find_class_id(mat_add_id), egraph.find_class_id(mat_axpy_id));
}