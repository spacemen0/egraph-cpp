#include "cost_functions/op_costs.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

using namespace egraph;

TEST(OpCostsTest, ComputeAddCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op("Add", {id_a, id_a});

    Cost cost = node.compute_local_cost(egraph, {});
    EXPECT_DOUBLE_EQ(cost, 9.0);

    // Direct invocation with Operand vector
    EXPECT_DOUBLE_EQ(compute_add_cost(OpRegistry::op("Add"), {{.rows = 3, .cols = 3}}), 9.0);
}

TEST(OpCostsTest, ComputeMulCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node = make_op("Mul", {id_x, id_y});

    Cost cost = node.compute_local_cost(egraph, {});
    // 2.0 * rows1 * cols1 * cols2 = 2.0 * 3 * 2 * 3 = 36.0
    EXPECT_DOUBLE_EQ(cost, 36.0);

    EXPECT_DOUBLE_EQ(compute_mul_cost(OpRegistry::op("Mul"), {{.rows = 3, .cols = 2}, {.rows = 2, .cols = 3}}), 36.0);
}

TEST(OpCostsTest, ComputeTrCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op("Tr", {id_x});

    Cost cost = node.compute_local_cost(egraph, {});
    EXPECT_DOUBLE_EQ(cost, 6.0);

    EXPECT_DOUBLE_EQ(compute_tr_cost(OpRegistry::op("Tr"), {{.rows = 3, .cols = 2}}), 6.0);
}

TEST(OpCostsTest, ComputeInvCostGeneralSquare) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3 general non-singular
    ENode node = make_op("Inv", {id_a});

    Cost cost = node.compute_local_cost(egraph, {});
    // 24.0 * 3^3 = 648.0
    EXPECT_DOUBLE_EQ(cost, 648.0);

    EXPECT_DOUBLE_EQ(compute_inv_cost(OpRegistry::op("Inv"), {{.rows = 3, .cols = 3}}), 648.0);
}

TEST(OpCostsTest, ComputeInvCostTriangularSquare) {
    PropertyTable pt;
    pt.add_or_update_property_entry(
        "U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true, .is_non_singular = true}});
    EGraph egraph(pt);
    Id id_u = egraph.add_node(make_symbol("U"));
    ENode node = make_op("Inv", {id_u});

    Cost cost = node.compute_local_cost(egraph, {});
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(cost, 9.0);

    Operand tri_operand{.rows = 3, .cols = 3};
    tri_operand.property.flags.is_upper_triangular = true;
    EXPECT_DOUBLE_EQ(compute_inv_cost(OpRegistry::op("Inv"), {tri_operand}), 9.0);
}

TEST(OpCostsTest, ComputeMinusCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op("Minus", {id_a, id_a});

    Cost cost = node.compute_local_cost(egraph, {});
    EXPECT_DOUBLE_EQ(cost, 9.0);

    EXPECT_DOUBLE_EQ(compute_minus_cost(OpRegistry::op("Minus"), {{.rows = 3, .cols = 3}}), 9.0);
}

TEST(OpCostsTest, ComputeScaleCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op("Scale", {id_x});

    Cost cost = node.compute_local_cost(egraph, {});
    EXPECT_DOUBLE_EQ(cost, 6.0);

    EXPECT_DOUBLE_EQ(compute_scale_cost(OpRegistry::op("Scale"), {{.rows = 3, .cols = 2}}), 6.0);
}

TEST(OpCostsTest, ComputePotrfCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_v = egraph.add_node(make_symbol("V")); // 3x3 positive definite
    ENode node = make_op("Potrf_L", {id_v});

    Cost cost = node.compute_local_cost(egraph, {});
    // (1.0 / 3.0) * 3^3 = 9.0
    EXPECT_DOUBLE_EQ(cost, 9.0);

    EXPECT_DOUBLE_EQ(compute_potrf_l_potrf_u_cost(OpRegistry::op("Potrf_L"), {{.rows = 3, .cols = 3}}), 9.0);
}

TEST(OpCostsTest, ComputeGemvCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    ENode node_n = make_op("Gemv_N", {id_x, id_y});
    ENode node_t = make_op("Gemv_T", {id_x, id_y});

    Cost cost_n = node_n.compute_local_cost(egraph, {});
    Cost cost_t = node_t.compute_local_cost(egraph, {});
    EXPECT_DOUBLE_EQ(cost_n, 12.0); // 2 * 3 * 2
    EXPECT_DOUBLE_EQ(cost_t, 12.0);

    EXPECT_DOUBLE_EQ(compute_gemv_n_cost(OpRegistry::op("Gemv_N"), {{.rows = 3, .cols = 2}}), 12.0);
    EXPECT_DOUBLE_EQ(compute_gemv_t_cost(OpRegistry::op("Gemv_T"), {{.rows = 3, .cols = 2}}), 12.0);
}

TEST(OpCostsTest, ComputeTrtriCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A")); // 3x3
    ENode node = make_op("Trtri", {id_a});

    Cost cost = node.compute_local_cost(egraph, {});
    // (1.0 / 3.0) * 27 = 9.0
    EXPECT_DOUBLE_EQ(cost, 9.0);

    EXPECT_DOUBLE_EQ(compute_trtri_cost(OpRegistry::op("Trtri"), {{.rows = 3, .cols = 3}}), 9.0);
}

TEST(OpCostsTest, ComputeSyrkCostNumeric) {
    EGraph egraph(get_property_table());
    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    ENode node = make_op("Syrk_T", {id_x});

    Cost cost = node.compute_local_cost(egraph, {});
    // Syrk_T cost on 3x2 matrix: 1.0 * n * (n + 1) * k = 2 * 3 * 3 = 18.0
    EXPECT_DOUBLE_EQ(cost, 18.0);

    EXPECT_DOUBLE_EQ(compute_syrk_n_syrk_t_cost(OpRegistry::op("Syrk_T"), {{.rows = 3, .cols = 2}}), 18.0);
}

TEST(OpCostsTest, ComputeMulCostTriangularLeftNumeric) {
    PropertyTable pt;
    pt.add_or_update_property_entry("U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true}});
    pt.add_or_update_property_entry("B", {.shape = {3, 2}});
    EGraph egraph(pt);
    Id id_u = egraph.add_node(make_symbol("U"));
    Id id_b = egraph.add_node(make_symbol("B"));
    ENode node = make_op("Mul", {id_u, id_b});

    Cost cost = node.compute_local_cost(egraph, {});
    // Left TRMM: 1.0 * 3 * 3 * 2 = 18.0 (vs GEMM: 2.0 * 3 * 3 * 2 = 36.0)
    EXPECT_DOUBLE_EQ(cost, 18.0);
}

TEST(OpCostsTest, ComputeMulCostTriangularRightNumeric) {
    PropertyTable pt;
    pt.add_or_update_property_entry("B", {.shape = {2, 3}});
    pt.add_or_update_property_entry("U", {.shape = {3, 3}, .flags = {.is_upper_triangular = true}});
    EGraph egraph(pt);
    Id id_b = egraph.add_node(make_symbol("B"));
    Id id_u = egraph.add_node(make_symbol("U"));
    ENode node = make_op("Mul", {id_b, id_u});

    Cost cost = node.compute_local_cost(egraph, {});
    // Right TRMM: 1.0 * 2 * 3 * 3 = 18.0 (vs GEMM: 2.0 * 2 * 3 * 3 = 36.0)
    EXPECT_DOUBLE_EQ(cost, 18.0);
}

TEST(OpCostsTest, ComputeCostThrowsOnMissingBinding) {
    EGraph egraph(get_property_table());
    Id id_m = egraph.add_node(make_symbol("M")); // AxB symbolic
    ENode node = make_op("Add", {id_m, id_m});

    // Without bindings, computing cost of symbolic op must throw std::runtime_error
    EXPECT_THROW(node.compute_local_cost(egraph, {}), std::runtime_error);

    // With complete bindings, it succeeds
    SizeBindings bindings = {{"A", 10}, {"B", 20}};
    Cost cost = node.compute_local_cost(egraph, bindings);
    EXPECT_DOUBLE_EQ(cost, 200.0);
}

TEST(OpCostsTest, PluggableCustomOpCost) {
    auto &registry = OpRegistry::instance();
    Op custom_op = registry.register_op("CustomNorm", true, [](const EGraph &, const std::vector<Id> &) {
        MatrixProperty p;
        p.shape = Shape{1, 1};
        return AnalysisData{p};
    }, [](Op, const std::vector<Operand> &) -> Cost {
        return 42.0;
    });

    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A"));
    ENode node{Children{id_a}, custom_op};

    EXPECT_DOUBLE_EQ(node.compute_local_cost(egraph, {}), 42.0);
}

TEST(OpCostsTest, OpWithoutCostThrows) {
    auto &registry = OpRegistry::instance();
    Op op_no_cost = registry.register_op("OpNoCost", true, [](const EGraph &, const std::vector<Id> &) {
        return AnalysisData{MatrixProperty{}};
    }, nullptr);

    EGraph egraph(get_property_table());
    Id id_a = egraph.add_node(make_symbol("A"));
    ENode node{Children{id_a}, op_no_cost};

    EXPECT_THROW(node.compute_local_cost(egraph, {}), std::invalid_argument);
}
