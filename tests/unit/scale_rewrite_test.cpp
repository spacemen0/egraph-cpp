#include "e_graph.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST(ScaleRewrite, ScaleCollapseDoubles) {
    EGraph egraph(get_property_table());

    // Scale(Scale(A, 1.5), 2.0) -> Scale(A, 3)
    Id id = egraph.add_expression(Expression("Scale(Scale(A, 1.5), 2.0)"));

    Rewriter rewriter(egraph, {scale_collapse}, RewriteConfig{.node_limit = 100});
    rewriter.apply_rewrites();

    Id id_expected = egraph.add_expression(Expression("Scale(A, 3)"));
    EXPECT_EQ(egraph.find_class_id(id), egraph.find_class_id(id_expected));
}

TEST(ScaleRewrite, ScaleCombineDoubles) {
    EGraph egraph(get_property_table());

    // Scale(A, 0.5) + Scale(A, 0.5) -> Scale(A, 1) -> A
    Id id = egraph.add_expression(Expression("Scale(A, 0.5) + Scale(A, 0.5)"));

    Rewriter rewriter(egraph, {scale_combine, scale_one}, RewriteConfig{.node_limit = 100});
    rewriter.apply_rewrites();

    Id id_expected = egraph.add_expression(Expression("A"));
    EXPECT_EQ(egraph.find_class_id(id), egraph.find_class_id(id_expected));
}

TEST(ScaleRewrite, ScaleCombineImplicitDoubles) {
    EGraph egraph(get_property_table());

    // Scale(A, 0.5) + A -> Scale(A, 1.5)
    Id id = egraph.add_expression(Expression("Scale(A, 0.5) + A"));

    Rewriter rewriter(egraph, {scale_combine_implicit}, RewriteConfig{.node_limit = 100});
    rewriter.apply_rewrites();

    Id id_expected = egraph.add_expression(Expression("Scale(A, 1.5)"));
    EXPECT_EQ(egraph.find_class_id(id), egraph.find_class_id(id_expected));
}

TEST(ScaleRewrite, ScaleInverse) {
    EGraph egraph(get_property_table());

    // Inv(Scale(A, 2.0)) -> Scale(Inv(A), 0.5)
    Id id = egraph.add_expression(Expression("Inv(Scale(A, 2.0))"));

    Rewriter rewriter(egraph, {scale_inverse}, RewriteConfig{.node_limit = 100});
    rewriter.apply_rewrites();

    Id id_expected = egraph.add_expression(Expression("Scale(Inv(A), 0.5)"));
    EXPECT_EQ(egraph.find_class_id(id), egraph.find_class_id(id_expected));
}

TEST(ScaleRewrite, ComplexScaleProduct) {
    EGraph egraph(get_property_table());

    // Inv(Scale(A, 3.0)) * Scale(Z, 3.0) -> Inv(A) * Z
    // Both A and Z are 3x3 in the default property table.
    Id id = egraph.add_expression(Expression("Inv(Scale(A, 3.0)) * Scale(Z, 3.0)"));

    Rewriter rewriter(egraph, {scale_inverse, scale_mul_distribute_left, scale_mul_distribute_right, scale_collapse, scale_one}, RewriteConfig{.node_limit = 100});
    rewriter.apply_rewrites();

    Id id_expected = egraph.add_expression(Expression("Inv(A) * Z"));
    EXPECT_EQ(egraph.find_class_id(id), egraph.find_class_id(id_expected));
}
