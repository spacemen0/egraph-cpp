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

TEST(Rewrite, TrsmLN_LeftSolve) {
    auto pt = get_property_table();

    MatrixProperty prop_a;
    prop_a.shape = {3, 3};
    prop_a.flags.is_non_singular = true;
    prop_a.flags.is_lower_triangular = true;
    pt.add_or_update_property_entry("a", prop_a);

    MatrixProperty prop_b;
    prop_b.shape = {3, 2};
    prop_b.flags.is_non_singular = true;
    pt.add_or_update_property_entry("b", prop_b);

    EGraph egraph(std::move(pt));

    Id id_expr = egraph.add_expression(Expression("Inv(a) * b"));

    Rewriter rewriter(egraph, {trsm_ln}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_solve = egraph.add_expression(Expression("Trsm_LN(a, b)"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_solve));
    EXPECT_EQ(
        std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape,
        std::make_pair(Size(3), Size(2)));
}

TEST(Rewrite, TrsmRN_RightSolve) {
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

    Rewriter rewriter(egraph, {trsm_rn}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_trsm_rn = egraph.add_expression(Expression("Trsm_RN(a, b)"));

    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_trsm_rn));
    EXPECT_EQ(
        std::get<MatrixProperty>(egraph.get_class_analysis_data(id_expr).property).shape,
        std::make_pair(Size(2), Size(3)));
}

TEST(Rewrite, CholeLRewrite) {
    EGraph egraph(get_property_table());

    Id id_expr = egraph.add_expression(Expression("Inv(V)"));

    Rewriter rewriter(egraph, {cholel_invert}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_cholel = egraph.add_expression(Expression("Tr(Inv(Get(CholeL(V), 0))) * Inv(Get(CholeL(V), 0))"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_cholel));
}

TEST(Rewrite, CholeLToCholeURewrite) {
    EGraph egraph(get_property_table());

    Id id_cholel = egraph.add_expression(Expression("Get(CholeL(V), 0)"));

    Rewriter rewriter(egraph, {cholel_to_choleu}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_choleu = egraph.add_expression(Expression("Tr(Get(CholeU(V), 0))"));
    EXPECT_EQ(egraph.find_class_id(id_cholel), egraph.find_class_id(id_choleu));
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

    bool changed1 = rewriter.apply_rewrites(1);
    EXPECT_TRUE(changed1);

    bool changed2 = rewriter.apply_rewrites(1);
    EXPECT_FALSE(changed2);

    bool changed3 = rewriter.apply_rewrites(1);
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

TEST(Rewrite, DisableKernelPropagatesToSymbolicOps) {
    // When both Potrf_L and Potrf_U are disabled, all Cholesky lowering paths are blocked -> both CholeL and CholeU are disabled
    auto effective_disabled_cholesky = Rewriter::compute_effective_disabled_ops({Op::Potrf_L, Op::Potrf_U});
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::Potrf_L));
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::Potrf_U));
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::CholeL));
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::CholeU));
    EXPECT_FALSE(effective_disabled_cholesky.contains(Op::QR));

    // When only Potrf_L is disabled, CholeL is NOT disabled because it can transition to CholeU via cholel_to_choleu and use Potrf_U
    auto effective_disabled_potrf_l = Rewriter::compute_effective_disabled_ops({Op::Potrf_L});
    EXPECT_TRUE(effective_disabled_potrf_l.contains(Op::Potrf_L));
    EXPECT_FALSE(effective_disabled_potrf_l.contains(Op::CholeL));
    EXPECT_FALSE(effective_disabled_potrf_l.contains(Op::CholeU));

    // When only Potrf_U is disabled, CholeU is disabled, but CholeL has a direct lowering path via Potrf_L
    auto effective_disabled_potrf_u = Rewriter::compute_effective_disabled_ops({Op::Potrf_U});
    EXPECT_TRUE(effective_disabled_potrf_u.contains(Op::Potrf_U));
    EXPECT_TRUE(effective_disabled_potrf_u.contains(Op::CholeU));
    EXPECT_FALSE(effective_disabled_potrf_u.contains(Op::CholeL));

    // Geqrf -> QR (only single lowering path exists)
    auto effective_disabled_geqrf = Rewriter::compute_effective_disabled_ops({Op::Geqrf});
    EXPECT_TRUE(effective_disabled_geqrf.contains(Op::Geqrf));
    EXPECT_TRUE(effective_disabled_geqrf.contains(Op::QR));
    EXPECT_FALSE(effective_disabled_geqrf.contains(Op::CholeL));
    EXPECT_FALSE(effective_disabled_geqrf.contains(Op::CholeU));

    // Both Cholesky and QR kernels disabled
    auto effective_disabled_all_factorizations =
        Rewriter::compute_effective_disabled_ops({Op::Potrf_L, Op::Potrf_U, Op::Geqrf});
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Potrf_L));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Potrf_U));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::CholeL));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::CholeU));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Geqrf));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::QR));

    // Empty disabled_ops -> nothing disabled
    auto effective_disabled_none = Rewriter::compute_effective_disabled_ops({});
    EXPECT_TRUE(effective_disabled_none.empty());

    // Custom multi-hop rules inspection: CholeL -> QR -> Geqrf
    std::vector<Rewrite> custom_multi_hop_rules = {
        make_rewrite("cholel_to_qr", "CholeL(?a)", "QR(?a)", false),
        make_rewrite("qr_to_geqrf", "QR(?a)", "Geqrf(?a)", false),
    };
    auto effective_disabled_custom = Rewriter::compute_effective_disabled_ops({Op::Geqrf}, &custom_multi_hop_rules);
    EXPECT_TRUE(effective_disabled_custom.contains(Op::Geqrf));
    EXPECT_TRUE(effective_disabled_custom.contains(Op::QR));
    EXPECT_TRUE(effective_disabled_custom.contains(Op::CholeL));

    // Verify expansion_set filtering in Rewriter when both Cholesky kernels are disabled
    EGraph egraph_instance(get_property_table());
    Rewriter rewriter_cholesky(egraph_instance, expansion_set, EGraphConfig{.disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
    const auto &filtered_rules_cholesky = rewriter_cholesky.get_rewrites();
    for (const auto &rule : filtered_rules_cholesky) {
        EXPECT_FALSE(rule.lhs.contains_op(Op::CholeL) || rule.rhs.contains_op(Op::CholeL))
            << "Rule " << rule.name << " still contains CholeL despite Cholesky kernels being disabled";
        EXPECT_FALSE(rule.lhs.contains_op(Op::CholeU) || rule.rhs.contains_op(Op::CholeU))
            << "Rule " << rule.name << " still contains CholeU despite Cholesky kernels being disabled";
    }
    // Should still have QR rules
    bool has_qr_rule = std::any_of(filtered_rules_cholesky.begin(), filtered_rules_cholesky.end(), [](const auto &rule) {
        return rule.lhs.contains_op(Op::QR) || rule.rhs.contains_op(Op::QR);
    });
    EXPECT_TRUE(has_qr_rule);

    Rewriter rewriter_geqrf(egraph_instance, expansion_set, EGraphConfig{.disabled_ops = {Op::Geqrf}});
    const auto &filtered_rules_geqrf = rewriter_geqrf.get_rewrites();
    for (const auto &rule : filtered_rules_geqrf) {
        EXPECT_FALSE(rule.lhs.contains_op(Op::QR) || rule.rhs.contains_op(Op::QR))
            << "Rule " << rule.name << " still contains QR despite Geqrf being disabled";
    }
    // Should still have CholeL rules
    bool has_cholel_rule = std::any_of(filtered_rules_geqrf.begin(), filtered_rules_geqrf.end(), [](const auto &rule) {
        return rule.lhs.contains_op(Op::CholeL) || rule.rhs.contains_op(Op::CholeL);
    });
    EXPECT_TRUE(has_cholel_rule);
}

TEST(Rewrite, DisablingKernelPreventsSymbolicGenerationInEGraph) {
    // 1. Without disabled_ops, Inv(V) expands to CholeL
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(V)"));
        Rewriter rewriter_instance(egraph_instance, expansion_set, EGraphConfig{.rewrite = {.node_limit = 100}});
        rewriter_instance.apply_rewrites();

        bool has_cholel_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::CholeL) {
                    has_cholel_node = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(has_cholel_node);
    }

    // 2. With both Potrf_L and Potrf_U disabled, Inv(V) MUST NOT produce CholeL
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(V)"));
        Rewriter rewriter_instance(
            egraph_instance, expansion_set,
            EGraphConfig{.rewrite = {.node_limit = 100}, .disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
        rewriter_instance.apply_rewrites();

        bool has_cholel_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::CholeL) {
                    has_cholel_node = true;
                    break;
                }
            }
        }
        EXPECT_FALSE(has_cholel_node);
    }

    // 3. With Geqrf disabled, Inv(A) MUST NOT produce QR
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(A)"));
        Rewriter rewriter_instance(
            egraph_instance, expansion_set,
            EGraphConfig{.rewrite = {.node_limit = 100}, .disabled_ops = {Op::Geqrf}});
        rewriter_instance.apply_rewrites();

        bool has_qr_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::QR) {
                    has_qr_node = true;
                    break;
                }
            }
        }
        EXPECT_FALSE(has_qr_node);
    }
}

TEST(Rewrite, DynamicSetConfigReFiltersRules) {
    EGraph egraph_instance(get_property_table());
    Rewriter rewriter_instance(egraph_instance, expansion_set);
    EXPECT_EQ(rewriter_instance.get_rewrites().size(), expansion_set.size());

    // Dynamically disable both Potrf_L and Potrf_U
    rewriter_instance.set_config(EGraphConfig{.disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
    EXPECT_LT(rewriter_instance.get_rewrites().size(), expansion_set.size());
    for (const auto &rule : rewriter_instance.get_rewrites()) {
        EXPECT_FALSE(rule.lhs.contains_op(Op::CholeL) || rule.rhs.contains_op(Op::CholeL));
    }

    // Re-enable everything
    rewriter_instance.set_config(EGraphConfig{.disabled_ops = {}});
    EXPECT_EQ(rewriter_instance.get_rewrites().size(), expansion_set.size());
}

TEST(Rewrite, DynamicRulesFilteringAndTraversal) {
    EGraph egraph_instance(get_property_table());

    // 1. When Gemm_NN is disabled, gemm_without_c (dynamic applier rule) must be filtered
    Rewriter rewriter_gemm(egraph_instance, lowering_set, EGraphConfig{.disabled_ops = {Op::Gemm_NN}});
    for (const auto &rule : rewriter_gemm.get_rewrites()) {
        EXPECT_NE(rule.name, "gemm_without_c");
        EXPECT_FALSE(rule.contains_op(Op::Gemm_NN));
    }

    // 2. Dynamic rule producing a symbolic op:
    auto custom_dynamic_expansion = make_rewrite(
        "custom_dynamic_cholel", "Inv(?a)", "Dynamic", false, nullptr,
        [](EGraph &, const Substitution &, Id id) { return std::make_pair(id, false); },
        {Op::CholeL});
    EXPECT_TRUE(custom_dynamic_expansion.contains_op(Op::CholeL));

    // When both Potrf_L and Potrf_U are disabled, compute_effective_disabled_ops includes CholeL
    auto effective_disabled_cholesky = Rewriter::compute_effective_disabled_ops({Op::Potrf_L, Op::Potrf_U});
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::CholeL));

    // Rewriter filtering prunes the custom dynamic expansion rule
    Rewriter rewriter_dyn_expansion(
        egraph_instance, {custom_dynamic_expansion}, EGraphConfig{.disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
    EXPECT_TRUE(rewriter_dyn_expansion.get_rewrites().empty());

    // 3. Dynamic lowering rule:
    // Custom rule dynamically lowers CholeL into Potrf_L via an applier
    auto custom_dynamic_lowering = make_rewrite(
        "custom_dynamic_potrf_l", "CholeL(?a)", "Dynamic", false, nullptr,
        [](EGraph &, const Substitution &, Id id) { return std::make_pair(id, false); },
        {Op::Potrf_L});
    EXPECT_TRUE(custom_dynamic_lowering.contains_op(Op::Potrf_L));
    EXPECT_TRUE(custom_dynamic_lowering.contains_op(Op::CholeL));

    // When Potrf_L is disabled, Rewriter filtering prunes the dynamic lowering rule
    Rewriter rewriter_dyn_lowering(egraph_instance, {custom_dynamic_lowering}, EGraphConfig{.disabled_ops = {Op::Potrf_L}});
    EXPECT_TRUE(rewriter_dyn_lowering.get_rewrites().empty());
}