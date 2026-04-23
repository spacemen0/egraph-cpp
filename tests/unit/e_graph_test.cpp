#include "e_graph.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST_F(EGraphTest, AddAndLookUpNode) {

    Id id1 = egraph.add_node(sym_x);
    std::optional<Id> lookup1 = egraph.find_node_id(sym_x);
    EXPECT_TRUE(lookup1.has_value());
    EXPECT_EQ(lookup1.value(), id1);

    Id id2 = egraph.add_node(sym_x);
    EXPECT_EQ(id1, id2);

    Id id3 = egraph.add_node(sym_y);
    EXPECT_NE(id1, id3);
}

TEST_F(EGraphTest, BreakWithInvalidChildId) {
    Id fake_id = 999999;
    ENode dangerous_node = make_op(Op::Minus, Children{fake_id, fake_id});
    EXPECT_ANY_THROW(egraph.add_node(dangerous_node));
}

TEST_F(EGraphTest, UnionClasses) {

    Id id1 = egraph.add_node(sym_z);
    Id id2 = egraph.add_node(sym_a);

    bool merged = egraph.union_classes(id1, id2);
    EXPECT_TRUE(merged);

    merged = egraph.union_classes(id1, id2);
    EXPECT_FALSE(merged);
    egraph.rebuild();
    auto root1 = egraph.find_node_id(sym_z);
    auto root2 = egraph.find_node_id(sym_a);
    EXPECT_EQ(root1, root2);
}

TEST_F(EGraphTest, RebuildParentsBasic) {

    auto id_node1 = egraph.add_expression(Expression("D + D"));
    auto id_node2 = egraph.add_expression(Expression("D + W"));

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));

    egraph.union_classes(egraph.find_node_id(sym_d).value(), egraph.find_node_id(sym_w).value());

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST_F(EGraphTest, RebuildMultiLevelParents) {

    Id id_add_ab = egraph.add_expression(Expression("A + Z"));
    Id id_add_ac = egraph.add_expression(Expression("A + A"));
    EXPECT_NE(egraph.find_class_id(id_add_ab), egraph.find_class_id(id_add_ac));

    Id id_mul1 = egraph.add_expression(Expression("(A + Z) * Z"));
    Id id_mul2 = egraph.add_expression(Expression("(A + A) * Z"));
    EXPECT_NE(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));

    Id id_nested1 = egraph.add_expression(Expression("((A + Z) * Z) + A"));
    Id id_nested2 = egraph.add_expression(Expression("((A + A) * Z) + A"));
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    egraph.union_classes(egraph.find_node_id(sym_a).value(), egraph.find_node_id(sym_z).value());

    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    Id root_nested = egraph.find_class_id(id_nested1);
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested1));
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested2));
}

TEST_F(EGraphTest, RebuildCleanUpEClass) {

    Id x = egraph.add_node(sym_a);
    Id y = egraph.add_node(sym_z);

    ENode node_x = make_op(Op::Minus, {x, x});
    Id neg_x = egraph.add_node(node_x);

    ENode node_y = make_op(Op::Minus, {y, y});
    Id neg_y = egraph.add_node(node_y);

    EXPECT_NE(egraph.find_class_id(neg_x), egraph.find_class_id(neg_y));

    egraph.union_classes(x, y);
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(neg_x), egraph.find_class_id(neg_y));

    EXPECT_EQ(egraph.get_class_nodes(egraph.find_class_id(neg_x)).size(), 1);
}

TEST_F(EGraphTest, AddExpression) {

    Expression expr("(X * Y) + Tr(Z)");
    Id expr_id = egraph.add_expression(expr);

    EXPECT_TRUE(egraph.find_node_id(sym_x).has_value());
    EXPECT_TRUE(egraph.find_node_id(sym_y).has_value());
    EXPECT_TRUE(egraph.find_node_id(sym_z).has_value());

    auto mul_xy = make_op(Op::Mul, Children{egraph.find_node_id(sym_x).value(), egraph.find_node_id(sym_y).value()});
    auto transpose_z = make_op(Op::Tr, Children{egraph.find_node_id(sym_z).value()});
    EXPECT_TRUE(egraph.find_node_id(mul_xy).has_value());
    EXPECT_TRUE(egraph.find_node_id(transpose_z).has_value());

    auto add_expr =
        make_op(Op::Add, Children{egraph.find_node_id(mul_xy).value(), egraph.find_node_id(transpose_z).value()});
    EXPECT_TRUE(egraph.find_node_id(add_expr).has_value());
    EXPECT_EQ(egraph.find_node_id(add_expr).value(), expr_id);

    Expression expr_dup("(X * Y) + Tr(Z)");
    Id expr_dup_id = egraph.add_expression(expr_dup);

    EXPECT_EQ(expr_id, expr_dup_id);

    Expression expr_diff("(X * Y) + Inv(Z)");
    Id expr_diff_id = egraph.add_expression(expr_diff);

    EXPECT_NE(expr_id, expr_diff_id);
}

TEST_F(EGraphTest, ENodeMatching) {

    Id id1 = egraph.add_node(sym_x);

    auto node = make_op(Op::Minus, Children{id1, id1});
    Id id2 = egraph.add_node(node);

    EXPECT_TRUE(egraph.find_node_id(node).has_value());
    EXPECT_EQ(egraph.find_node_id(node).value(), id2);

    auto node_diff = make_op(Op::Minus, Children{id1 + 1, id1 + 1});
    EXPECT_FALSE(egraph.find_node_id(node_diff).has_value());
}

TEST_F(EGraphTest, MultipleExpressionsShareCommonSubexpression) {

    Id expr1 = egraph.add_expression(Expression("(X * Y) + Tr(Z)"));
    Id expr2 = egraph.add_expression(Expression("(X * Y) + Inv(Z)"));

    auto id_x = egraph.find_node_id(sym_x);
    auto id_y = egraph.find_node_id(sym_y);
    ASSERT_TRUE(id_x.has_value());
    ASSERT_TRUE(id_y.has_value());

    auto mul_xy = make_op(Op::Mul, Children{id_x.value(), id_y.value()});
    auto mul_xy_id = egraph.find_node_id(mul_xy);
    ASSERT_TRUE(mul_xy_id.has_value());

    auto mul_xy_class = egraph.find_class_id(mul_xy_id.value());
    auto parents = egraph.get_class_parents(mul_xy_class);

    EXPECT_NE(expr1, expr2);
    EXPECT_GE(parents.size(), 2);

    auto add1 = make_op(
        Op::Add,
        Children{
            mul_xy_id.value(), egraph.find_node_id(make_op(Op::Tr, {egraph.find_node_id(sym_z).value()})).value()});
    auto add2 = make_op(
        Op::Add,
        Children{
            mul_xy_id.value(), egraph.find_node_id(make_op(Op::Inv, {egraph.find_node_id(sym_z).value()})).value()});
    EXPECT_TRUE(egraph.find_node_id(add1).has_value());
    EXPECT_TRUE(egraph.find_node_id(add2).has_value());
}