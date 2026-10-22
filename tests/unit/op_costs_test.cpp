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
    ENode node = make_op("Add", {id_a, id_a});

    Cost cost = compute_add_cost(OpRegistry::op("Add"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeMulCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node = make_op("Mul", {id_x, id_y});

    Cost cost = compute_mul_cost(OpRegistry::op("Mul"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // 2.0 * rows1 * cols1 * cols2 = 2.0 * 3 * 2 * 3 = 36.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 36.0);
}

TEST(OpCostsTest, ComputeTrCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op("Tr", {id_x});

    Cost cost = compute_tr_cost(OpRegistry::op("Tr"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 6.0);
}

TEST(OpCostsTest, ComputeInvCostGeneralSquare) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3 general non-singular
    ENode node = make_op("Inv", {id_a});

    Cost cost = compute_inv_cost(OpRegistry::op("Inv"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // 24.0 * 3^3 = 648.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 648.0);
}

TEST(OpCostsTest, ComputeInvCostTriangularSquare) {
    PropertyTable pt;
    pt.add_or_update_property_entry(
        "U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true, .is_non_singular = true}});
    EGraph egraph(pt);
    Id id_u = egraph.add_node(make_symbol("U"));
    ENode node = make_op("Inv", {id_u});

    Cost cost = compute_inv_cost(OpRegistry::op("Inv"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeMinusCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op("Minus", {id_a, id_a});

    Cost cost = compute_minus_cost(OpRegistry::op("Minus"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeScaleCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op("Scale", {id_x});

    Cost cost = compute_scale_cost(OpRegistry::op("Scale"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 6.0);
}

TEST(OpCostsTest, ComputePotrfCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_v = egraph.add_node(make_symbol("V")); // 3x3 positive definite
    ENode node = make_op("Potrf_L", {id_v});

    Cost cost = compute_potrf_l_potrf_u_cost(OpRegistry::op("Potrf_L"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeGemvCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node_n = make_op("Gemv_N", {id_x, id_y});
    ENode node_t = make_op("Gemv_T", {id_x, id_y});

    Cost cost_n = compute_gemv_n_cost(OpRegistry::op("Gemv_N"), node_n, egraph, nullptr);
    Cost cost_t = compute_gemv_t_cost(OpRegistry::op("Gemv_T"), node_t, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost_n));
    ASSERT_TRUE(std::holds_alternative<double>(cost_t));
    EXPECT_DOUBLE_EQ(std::get<double>(cost_n), 12.0); // 2 * 3 * 2
    EXPECT_DOUBLE_EQ(std::get<double>(cost_t), 12.0);
}

TEST(OpCostsTest, ComputeTrtriCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op("Trtri", {id_a});

    Cost cost = compute_trtri_cost(OpRegistry::op("Trtri"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // (1.0 / 3.0) * 27 = 9.0
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 9.0);
}

TEST(OpCostsTest, ComputeMulCostTransposeNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X"));         // 3x2
    Id id_tr_x = egraph.add_node(make_op("Tr", {id_x})); // 2x3
    ENode node = make_op("Mul", {id_tr_x, id_x});        // Tr(X) * X: 2x2, inner 3

    Cost cost = compute_mul_cost(OpRegistry::op("Mul"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // SYRK cost: 1.0 * n * (n + 1) * k = 2 * 3 * 3 = 18.0 (vs GEMM: 2 * 2 * 3 * 2 = 24.0)
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 18.0);
}

TEST(OpCostsTest, ComputeMulCostTriangularLeftNumeric) {
    PropertyTable pt;
    pt.add_or_update_property_entry("U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true}});
    pt.add_or_update_property_entry("B", {.shape = {3, 2}});
    EGraph egraph(pt);
    Id id_u = egraph.add_node(make_symbol("U"));
    Id id_b = egraph.add_node(make_symbol("B"));
    ENode node = make_op("Mul", {id_u, id_b});

    Cost cost = compute_mul_cost(OpRegistry::op("Mul"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // Left TRMM: 1.0 * 3 * 3 * 2 = 18.0 (vs GEMM: 2.0 * 3 * 3 * 2 = 36.0)
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 18.0);
}

TEST(OpCostsTest, ComputeMulCostTriangularRightNumeric) {
    PropertyTable pt;
    pt.add_or_update_property_entry("B", {.shape = {2, 3}});
    pt.add_or_update_property_entry("U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true}});
    EGraph egraph(pt);
    Id id_b = egraph.add_node(make_symbol("B"));
    Id id_u = egraph.add_node(make_symbol("U"));
    ENode node = make_op("Mul", {id_b, id_u});

    Cost cost = compute_mul_cost(OpRegistry::op("Mul"), node, egraph, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(cost));
    // Right TRMM: 1.0 * 2 * 3 * 3 = 18.0 (vs GEMM: 2.0 * 2 * 3 * 3 = 36.0)
    EXPECT_DOUBLE_EQ(std::get<double>(cost), 18.0);
}
