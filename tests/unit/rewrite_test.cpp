#include "e_graph.h"
#include "lowering.h"
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

    Id id_cholel = egraph.add_expression(Expression("Tr(Inv(Get(Potrf_L(V), 0))) * Inv(Get(Potrf_L(V), 0))"));
    EXPECT_EQ(egraph.find_class_id(id_expr), egraph.find_class_id(id_cholel));
}

TEST(Rewrite, CholeLToCholeURewrite) {
    EGraph egraph(get_property_table());

    Id id_cholel = egraph.add_expression(Expression("Get(Potrf_L(V), 0)"));

    Rewriter rewriter(egraph, {cholel_to_choleu}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    Id id_choleu = egraph.add_expression(Expression("Tr(Get(Potrf_U(V), 0))"));
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

TEST(Rewrite, DisableKernelFiltersExpansionRules) {
    auto effective_disabled_cholesky = Rewriter::compute_effective_disabled_ops({Op::Potrf_L, Op::Potrf_U});
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::Potrf_L));
    EXPECT_TRUE(effective_disabled_cholesky.contains(Op::Potrf_U));
    EXPECT_FALSE(effective_disabled_cholesky.contains(Op::Geqrf));

    auto effective_disabled_potrf_l = Rewriter::compute_effective_disabled_ops({Op::Potrf_L});
    EXPECT_TRUE(effective_disabled_potrf_l.contains(Op::Potrf_L));
    EXPECT_FALSE(effective_disabled_potrf_l.contains(Op::Potrf_U));

    auto effective_disabled_geqrf = Rewriter::compute_effective_disabled_ops({Op::Geqrf});
    EXPECT_TRUE(effective_disabled_geqrf.contains(Op::Geqrf));
    EXPECT_FALSE(effective_disabled_geqrf.contains(Op::Potrf_L));
    EXPECT_FALSE(effective_disabled_geqrf.contains(Op::Potrf_U));

    auto effective_disabled_all_factorizations =
        Rewriter::compute_effective_disabled_ops({Op::Potrf_L, Op::Potrf_U, Op::Geqrf});
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Potrf_L));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Potrf_U));
    EXPECT_TRUE(effective_disabled_all_factorizations.contains(Op::Geqrf));

    auto effective_disabled_none = Rewriter::compute_effective_disabled_ops({});
    EXPECT_TRUE(effective_disabled_none.empty());

    // Verify expansion_set filtering in Rewriter when both Cholesky kernels are disabled
    EGraph egraph_instance(get_property_table());
    Rewriter rewriter_cholesky(
        egraph_instance, expansion_set, EGraphConfig{.disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
    const auto &filtered_rules_cholesky = rewriter_cholesky.get_rewrites();
    for (const auto &rule : filtered_rules_cholesky) {
        EXPECT_FALSE(rule.lhs.contains_op(Op::Potrf_L) || rule.rhs.contains_op(Op::Potrf_L))
            << "Rule " << rule.name << " still contains Potrf_L despite Cholesky kernels being disabled";
        EXPECT_FALSE(rule.lhs.contains_op(Op::Potrf_U) || rule.rhs.contains_op(Op::Potrf_U))
            << "Rule " << rule.name << " still contains Potrf_U despite Cholesky kernels being disabled";
    }
    // Should still have Geqrf rules
    bool has_geqrf_rule =
        std::any_of(filtered_rules_cholesky.begin(), filtered_rules_cholesky.end(), [](const auto &rule) {
        return rule.lhs.contains_op(Op::Geqrf) || rule.rhs.contains_op(Op::Geqrf);
    });
    EXPECT_TRUE(has_geqrf_rule);

    Rewriter rewriter_geqrf(egraph_instance, expansion_set, EGraphConfig{.disabled_ops = {Op::Geqrf}});
    const auto &filtered_rules_geqrf = rewriter_geqrf.get_rewrites();
    for (const auto &rule : filtered_rules_geqrf) {
        EXPECT_FALSE(rule.lhs.contains_op(Op::Geqrf) || rule.rhs.contains_op(Op::Geqrf))
            << "Rule " << rule.name << " still contains Geqrf despite Geqrf being disabled";
    }
    // Should still have Potrf_L rules
    bool has_potrf_l_rule = std::any_of(filtered_rules_geqrf.begin(), filtered_rules_geqrf.end(), [](const auto &rule) {
        return rule.lhs.contains_op(Op::Potrf_L) || rule.rhs.contains_op(Op::Potrf_L);
    });
    EXPECT_TRUE(has_potrf_l_rule);
}

TEST(Rewrite, DisablingKernelPreventsFactorizationInEGraph) {
    // 1. Without disabled_ops, Inv(V) expands to Potrf_L
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(V)"));
        Rewriter rewriter_instance(egraph_instance, expansion_set, EGraphConfig{.rewrite = {.node_limit = 100}});
        rewriter_instance.apply_rewrites();

        bool has_potrf_l_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::Potrf_L) {
                    has_potrf_l_node = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(has_potrf_l_node);
    }

    // 2. With both Potrf_L and Potrf_U disabled, Inv(V) MUST NOT produce Potrf_L
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(V)"));
        Rewriter rewriter_instance(
            egraph_instance, expansion_set,
            EGraphConfig{.rewrite = {.node_limit = 100}, .disabled_ops = {Op::Potrf_L, Op::Potrf_U}});
        rewriter_instance.apply_rewrites();

        bool has_potrf_l_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::Potrf_L) {
                    has_potrf_l_node = true;
                    break;
                }
            }
        }
        EXPECT_FALSE(has_potrf_l_node);
    }

    // 3. With Geqrf disabled, Inv(A) MUST NOT produce Geqrf
    {
        EGraph egraph_instance(get_property_table());
        egraph_instance.add_expression(Expression("Inv(A)"));
        Rewriter rewriter_instance(
            egraph_instance, expansion_set, EGraphConfig{.rewrite = {.node_limit = 100}, .disabled_ops = {Op::Geqrf}});
        rewriter_instance.apply_rewrites();

        bool has_geqrf_node = false;
        for (Id class_id : egraph_instance.get_all_class_ids()) {
            for (const ENode *node : egraph_instance.get_class_nodes(class_id)) {
                if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::Geqrf) {
                    has_geqrf_node = true;
                    break;
                }
            }
        }
        EXPECT_FALSE(has_geqrf_node);
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
        EXPECT_FALSE(rule.lhs.contains_op(Op::Potrf_L) || rule.rhs.contains_op(Op::Potrf_L));
        EXPECT_FALSE(rule.lhs.contains_op(Op::Potrf_U) || rule.rhs.contains_op(Op::Potrf_U));
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

    // 2. Dynamic rule producing an op:
    auto custom_dynamic_expansion = make_rewrite(
        "custom_dynamic_potrf_l", "Inv(?a)", "Dynamic", false, nullptr, [](EGraph &, const Substitution &, Id id) {
        return std::make_pair(id, false);
    }, {Op::Potrf_L});
    EXPECT_TRUE(custom_dynamic_expansion.contains_op(Op::Potrf_L));

    // Rewriter filtering prunes the custom dynamic expansion rule
    Rewriter rewriter_dyn_expansion(
        egraph_instance, {custom_dynamic_expansion}, EGraphConfig{.disabled_ops = {Op::Potrf_L}});
    EXPECT_TRUE(rewriter_dyn_expansion.get_rewrites().empty());

    // 3. Dynamic lowering rule:
    auto custom_dynamic_lowering = make_rewrite(
        "custom_dynamic_geqrf", "Inv(?a)", "Dynamic", false, nullptr, [](EGraph &, const Substitution &, Id id) {
        return std::make_pair(id, false);
    }, {Op::Geqrf});
    EXPECT_TRUE(custom_dynamic_lowering.contains_op(Op::Geqrf));

    // When Geqrf is disabled, Rewriter filtering prunes the dynamic lowering rule
    Rewriter rewriter_dyn_lowering(
        egraph_instance, {custom_dynamic_lowering}, EGraphConfig{.disabled_ops = {Op::Geqrf}});
    EXPECT_TRUE(rewriter_dyn_lowering.get_rewrites().empty());
}

TEST(Rewrite, TransposeSpdDiscoversSymmetryWithoutFullRank) {
    PropertyTable pt;
    // Non-full rank matrix R (e.g. rank-deficient 3x3)
    pt.add_or_update_property_entry("R", {.shape = std::make_pair(3, 3), .flags = {.is_full_rank = false}});
    // Full column rank matrix F (3x2, full rank)
    pt.add_or_update_property_entry("F", {.shape = std::make_pair(3, 2), .flags = {.is_full_rank = true}});

    EGraph egraph(std::move(pt));

    Id id_r_gram = egraph.add_expression(Expression("Tr(R) * R"));
    Id id_f_gram = egraph.add_expression(Expression("Tr(F) * F"));

    Rewriter rewriter(egraph, {transpose_spd2}, EGraphConfig{.rewrite = {.node_limit = 100}});
    bool changed = rewriter.apply_rewrites();
    EXPECT_TRUE(changed);

    // For R (rank-deficient): must discover symmetry and positive semi-definiteness, but NOT strict positive
    // definiteness
    const auto &r_prop = std::get<MatrixProperty>(egraph.get_class_analysis_data(id_r_gram).property);
    EXPECT_TRUE(r_prop.flags.is_symmetric);
    EXPECT_TRUE(r_prop.flags.is_positive_semi_definite);
    EXPECT_FALSE(r_prop.flags.is_positive_definite);

    // For F (full column rank): must discover symmetry, semi-definiteness, AND positive definiteness
    const auto &f_prop = std::get<MatrixProperty>(egraph.get_class_analysis_data(id_f_gram).property);
    EXPECT_TRUE(f_prop.flags.is_symmetric);
    EXPECT_TRUE(f_prop.flags.is_positive_semi_definite);
    EXPECT_TRUE(f_prop.flags.is_positive_definite);
}

TEST(Rewrite, TopDownTriangularityDiscoveredOnExistingChild) {
    PropertyTable pt;
    pt.add_or_update_property_entry(
        "Q", {.shape = {2, 2}, .flags = {.is_full_rank = true, .is_non_singular = true, .is_orthogonal = true}});
    pt.add_or_update_property_entry(
        "Identity", {.shape = {2, 2},
                     .flags = {
                         .is_diagonal = true,
                         .is_upper_triangular = true,
                         .is_lower_triangular = true,
                         .is_identity = true,
                         .is_full_rank = true,
                         .is_non_singular = true}});

    EGraph egraph(std::move(pt));

    // Child : Q * Tr(Q * Identity)
    // Parent expression: Tr(Tr(Q)) * Tr(Q)
    Id id_child = egraph.find_class_id(egraph.add_expression(Expression("Q * Tr(Q * Identity)")));
    Id id_parent = egraph.find_class_id(egraph.add_expression(Expression("Tr(Tr(Q)) * Tr(Q)")));

    const auto &child_prop_before = std::get<MatrixProperty>(egraph.get_class_analysis_data(id_child).property);
    const auto &parent_prop_before = std::get<MatrixProperty>(egraph.get_class_analysis_data(id_parent).property);
    EXPECT_FALSE(child_prop_before.flags.is_lower_triangular);
    EXPECT_FALSE(child_prop_before.flags.is_upper_triangular);
    EXPECT_FALSE(parent_prop_before.flags.is_upper_triangular);

    std::vector<Rewrite> rules = {
        orthogonal_transpose, orthogonal_transpose_right, mat_transpose_prod, mul_identity_left, mul_identity_right};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 200}});
    egraph.to_img("top_down_triangularity_before", "png");

    rewriter.apply_rewrites(1);

    egraph.to_img("top_down_triangularity_after", "png");
    Id parent_class = egraph.find_class_id(id_parent);
    const auto &parent_prop_iter1 = std::get<MatrixProperty>(egraph.get_class_analysis_data(parent_class).property);

    Id child_class = egraph.find_class_id(id_child);
    const auto &child_prop_iter1 = std::get<MatrixProperty>(egraph.get_class_analysis_data(child_class).property);

    EXPECT_TRUE(parent_prop_iter1.flags.is_upper_triangular);

    EXPECT_TRUE(child_prop_iter1.flags.is_lower_triangular);
}
