#include "cost_functions/op_costs.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

using namespace egraph;

TEST(OpCostsTest, SizeToSymbol) {
    EXPECT_EQ(size_to_symbol(Size(42)), "42");
    EXPECT_EQ(size_to_symbol(Size(0)), "0");
    EXPECT_EQ(size_to_symbol(Size("N")), "N");
    EXPECT_EQ(size_to_symbol(Size("batch_size")), "batch_size");
}

TEST(OpCostsTest, ComputeAddCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op(Op::Add, {id_a, id_a});

    Cost cost = compute_add_cost(Op::Add, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeAddCostSymbolic) {
    EGraph egraph(get_property_table());
    Id id_m = egraph.add_node(make_symbol("M")); // A x B
    ENode node = make_op(Op::Add, {id_m, id_m});

    Cost cost = compute_add_cost(Op::Add, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<SymbolicCost>(cost));
    const auto &sc = std::get<SymbolicCost>(cost);
    EXPECT_EQ(sc.size(), 1);
    EXPECT_DOUBLE_EQ(sc.at(Monomial({"A", "B"})), 1.0);
}

TEST(OpCostsTest, ComputeMulCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node = make_op(Op::Mul, {id_x, id_y});

    Cost cost = compute_mul_cost(Op::Mul, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // 2.0 * rows1 * cols1 * cols2 = 2.0 * 3 * 2 * 3 = 36.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 36.0);
}

TEST(OpCostsTest, ComputeMulCostSymbolic) {
    EGraph egraph(get_property_table_with_symbolic_shapes());
    Id id_a = egraph.add_node(make_symbol("A")); // a x b
    Id id_b = egraph.add_node(make_symbol("B")); // b x c
    ENode node = make_op(Op::Mul, {id_a, id_b});

    Cost cost = compute_mul_cost(Op::Mul, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<SymbolicCost>(cost));
    const auto &sc = std::get<SymbolicCost>(cost);
    EXPECT_EQ(sc.size(), 1);
    EXPECT_DOUBLE_EQ(sc.at(Monomial({"a", "b", "c"})), 2.0);
}

TEST(OpCostsTest, ComputeTrCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op(Op::Tr, {id_x});

    Cost cost = compute_tr_cost(Op::Tr, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 6.0);
}

TEST(OpCostsTest, ComputeInvCostGeneralSquare) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3 general non-singular
    ENode node = make_op(Op::Inv, {id_a});

    Cost cost = compute_inv_cost(Op::Inv, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // 8.0 * 3^3 = 216.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 216.0);
}

TEST(OpCostsTest, ComputeInvCostTriangularSquare) {
    PropertyTable pt;
    pt.add_or_update_property_entry("U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true, .is_non_singular = true}});
    EGraph egraph(pt);
    Id id_u = egraph.add_node(make_symbol("U"));
    ENode node = make_op(Op::Inv, {id_u});

    Cost cost = compute_inv_cost(Op::Inv, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}


TEST(OpCostsTest, ComputeMinusCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op(Op::Minus, {id_a, id_a});

    Cost cost = compute_minus_cost(Op::Minus, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeScaleCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op(Op::Scale, {id_x});

    Cost cost = compute_scale_cost(Op::Scale, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 6.0);
}

TEST(OpCostsTest, ComputePotrfCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_v = egraph.add_node(make_symbol("V")); // 3x3 positive definite
    ENode node = make_op(Op::Potrf_L, {id_v});

    Cost cost = compute_potrf_l_potrf_u_cost(Op::Potrf_L, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeGemvCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node_n = make_op(Op::Gemv_N, {id_x, id_y});
    ENode node_t = make_op(Op::Gemv_T, {id_x, id_y});

    Cost cost_n = compute_gemv_n_cost(Op::Gemv_N, node_n, egraph, nullptr);
    Cost cost_t = compute_gemv_t_cost(Op::Gemv_T, node_t, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost_n));
    ASSERT_TRUE(std::holds_alternative<double>(cost_t));
    EXPECT_DOUBLE_EQ(std::get<double>(cost_n), 12.0); // 2 * 3 * 2
    EXPECT_DOUBLE_EQ(std::get<double>(cost_t), 12.0);
}

TEST(OpCostsTest, ComputeSolCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3 general
    Id id_y = egraph.add_node(make_symbol("y")); // 3x1
    ENode node = make_op(Op::Sol, {id_a, id_y});

    Cost cost = compute_sol_cost(Op::Sol, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // General solve: (2/3)*n^3 + 2*n^2*k = (2/3)*27 + 2*9*1 = 18 + 18 = 36.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 36.0);

    // Triangular solve
    PropertyTable pt;
    pt.add_or_update_property_entry("L", {.shape = {3, 3}, .flags = {.is_lower_triangular = true, .is_non_singular = true}});
    pt.add_or_update_property_entry("b", {.shape = {3, 1}});
    EGraph g2(pt);
    Id id_l = g2.add_node(make_symbol("L"));
    Id id_b = g2.add_node(make_symbol("b"));
    ENode node_tri = make_op(Op::Sol, {id_l, id_b});

    Cost cost_tri = compute_sol_cost(Op::Sol, node_tri, g2, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost_tri));
    // Triangular solve: 1.0 * n^2 * k = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost_tri), 9.0);
}

TEST(OpCostsTest, ComputeTrtriCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op(Op::Trtri, {id_a});

    Cost cost = compute_trtri_cost(Op::Trtri, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 27 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, SolAndSolrHandleMixedShapes) {
    PropertyTable pt;
    pt.add_or_update_property_entry("A_num", {.shape = {3, 3}});
    pt.add_or_update_property_entry("B_sym", {.shape = {3, "k"}});
    EGraph g(pt);
    Id id_a = g.add_node(make_symbol("A_num"));
    Id id_b = g.add_node(make_symbol("B_sym"));

    ENode sol_node = make_op(Op::Sol, {id_a, id_b});
    Cost sol_cost;
    EXPECT_NO_THROW({ sol_cost = compute_sol_cost(Op::Sol, sol_node, g, nullptr); });
    EXPECT_TRUE(std::holds_alternative<SymbolicCost>(sol_cost));

    ENode solr_node = make_op(Op::SolR, {id_a, id_b});
    Cost solr_cost;
    EXPECT_NO_THROW({ solr_cost = compute_solr_cost(Op::SolR, solr_node, g, nullptr); });
    EXPECT_TRUE(std::holds_alternative<SymbolicCost>(solr_cost));
}

TEST(OpCostsTest, ComputeMulCostTransposeNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_tr_x = egraph.add_node(make_op(Op::Tr, {id_x})); // 2x3
    ENode node = make_op(Op::Mul, {id_tr_x, id_x}); // Tr(X) * X: 2x2, inner 3

    Cost cost = compute_mul_cost(Op::Mul, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // SYRK cost: 1.0 * n * (n + 1) * k = 2 * 3 * 3 = 18.0 (vs GEMM: 2 * 2 * 3 * 2 = 24.0)
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 18.0);
}

TEST(OpCostsTest, ComputeMulCostTransposeSymbolic) {
    EGraph egraph(get_property_table_with_symbolic_shapes());
    Id id_a = egraph.add_node(make_symbol("A")); // a x b
    Id id_tr_a = egraph.add_node(make_op(Op::Tr, {id_a})); // b x a
    ENode node = make_op(Op::Mul, {id_tr_a, id_a});

    Cost cost = compute_mul_cost(Op::Mul, node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<SymbolicCost>(cost));
    const auto &sc = std::get<SymbolicCost>(cost);
    EXPECT_EQ(sc.size(), 2);
    EXPECT_DOUBLE_EQ(sc.at(Monomial({"b", "b", "a"})), 1.0);
    EXPECT_DOUBLE_EQ(sc.at(Monomial({"b", "a"})), 1.0);
}
