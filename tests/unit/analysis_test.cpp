#include "analysis.h"
#include "e_graph.h"
#include "errors.h"
#include "extractor.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
using namespace egraph;

TEST(EGraph, ErrorConditions) {
    EGraph egraph(get_property_table());

    ENode unknown_var = make_symbol("UNKNOWN_VAR");
    EXPECT_THROW(egraph.add_node(unknown_var), AnalysisError);

    Id x = egraph.add_node(sym_x); // 3x2
    Id y = egraph.add_node(sym_y); // 2x3
    ENode mismatch_add = make_op(Op::Add, {x, y});
    EXPECT_THROW(egraph.add_node(mismatch_add), ShapeMismatchError);

    ENode mismatch_mul = make_op(Op::Mul, {x, x});
    EXPECT_THROW(egraph.add_node(mismatch_mul), ShapeMismatchError);

    ENode invalid_invert = make_op(Op::Inv, {x});
    EXPECT_THROW(egraph.add_node(invalid_invert), InvalidOperationError);
}

TEST(EGraph, BoundExtractionDoesNotMutateAnalysisData) {
    EGraph egraph(get_property_table());

    Id id_m = egraph.add_node(make_symbol("M"));
    Id id_tr_m = egraph.add_node(make_op(Op::Tr, {id_m}));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_tr_m, id_m}));

    const auto *m_prop_before = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_m).property);
    ASSERT_NE(m_prop_before, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::string>(m_prop_before->shape.first));
    EXPECT_TRUE(std::holds_alternative<std::string>(m_prop_before->shape.second));

    const auto *mul_prop_before = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_mul).property);
    ASSERT_NE(mul_prop_before, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::string>(mul_prop_before->shape.first));
    EXPECT_TRUE(std::holds_alternative<std::string>(mul_prop_before->shape.second));

    Extractor extractor(egraph);
    auto result = extractor.extract(id_mul, {{"A", 5}, {"B", 3}});
    EXPECT_TRUE(std::holds_alternative<double>(result.cost));
    EXPECT_EQ(std::get<double>(result.cost), 105.0);

    const auto *m_prop_after = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_m).property);
    ASSERT_NE(m_prop_after, nullptr);
    EXPECT_EQ(m_prop_after->shape, std::make_pair(Size(std::string("A")), Size(std::string("B"))));

    const auto *mul_prop_after = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_mul).property);
    ASSERT_NE(mul_prop_after, nullptr);
    EXPECT_EQ(mul_prop_after->shape, std::make_pair(Size(std::string("B")), Size(std::string("B"))));
}

TEST(MatrixAnalysisTest, EnforceHierarchyPermutation) {
    MatrixProperty p;
    p.shape = {3, 3};
    p.flags.is_permutation = true;
    MatrixAnalysis::enforce_hierarchy(p);

    EXPECT_TRUE(p.flags.is_orthogonal);
    EXPECT_TRUE(p.flags.is_non_singular);
    EXPECT_TRUE(p.flags.is_full_rank);
    EXPECT_TRUE(p.flags.has_orthonormal_columns);
}

TEST(MatrixAnalysisTest, EnforceHierarchySquareOrthonormalColumns) {
    MatrixProperty p;
    p.shape = {4, 4};
    p.flags.has_orthonormal_columns = true;
    MatrixAnalysis::enforce_hierarchy(p);

    EXPECT_TRUE(p.flags.is_orthogonal);
    EXPECT_TRUE(p.flags.is_non_singular);
    EXPECT_TRUE(p.flags.is_full_rank);
}

TEST(MatrixAnalysisTest, EnforceHierarchyTriangularCombinationIsDiagonal) {
    MatrixProperty p;
    p.shape = {3, 3};
    p.flags.is_upper_triangular = true;
    p.flags.is_lower_triangular = true;
    MatrixAnalysis::enforce_hierarchy(p);

    EXPECT_TRUE(p.flags.is_diagonal);
    EXPECT_TRUE(p.flags.is_symmetric);
}

TEST(MatrixAnalysisTest, EnforceHierarchyPositiveDefinite) {
    MatrixProperty p;
    p.shape = {3, 3};
    p.flags.is_positive_definite = true;
    MatrixAnalysis::enforce_hierarchy(p);

    EXPECT_TRUE(p.flags.is_symmetric);
    EXPECT_TRUE(p.flags.is_non_singular);
    EXPECT_TRUE(p.flags.is_full_rank);
    EXPECT_TRUE(p.flags.is_positive_semi_definite);
}

TEST(MatrixAnalysisTest, EnforceHierarchyZeroMatrix) {
    MatrixProperty p;
    p.shape = {2, 2};
    p.flags.is_zero = true;
    MatrixAnalysis::enforce_hierarchy(p);

    EXPECT_TRUE(p.flags.is_diagonal);
    EXPECT_FALSE(p.flags.is_non_singular);
}

TEST(MatrixAnalysisTest, MergeCombinesFlags) {
    AnalysisData d1{MatrixProperty{.shape = {2, 2}, .flags = {.is_symmetric = true}}};
    AnalysisData d2{MatrixProperty{.shape = {2, 2}, .flags = {.is_positive_definite = true}}};

    bool changed = MatrixAnalysis::merge(d1, d2);
    EXPECT_TRUE(changed);

    const auto *prop = std::get_if<MatrixProperty>(&d1.property);
    ASSERT_NE(prop, nullptr);
    EXPECT_TRUE(prop->flags.is_symmetric);
    EXPECT_TRUE(prop->flags.is_positive_definite);
    EXPECT_TRUE(prop->flags.is_non_singular);
}

TEST(MatrixAnalysisTest, MergeConflictingShapesThrows) {
    AnalysisData d1{MatrixProperty{.shape = {2, 3}}};
    AnalysisData d2{MatrixProperty{.shape = {3, 2}}};

    EXPECT_THROW(MatrixAnalysis::merge(d1, d2), ShapeMismatchError);
}

TEST(MatrixAnalysisTest, MergeMatrixWithTupleThrows) {
    AnalysisData d1{MatrixProperty{.shape = {2, 2}}};
    AnalysisData d2{TupleProperty{std::vector<MatrixProperty>{MatrixProperty{.shape = {2, 2}}}}};

    EXPECT_THROW(MatrixAnalysis::merge(d1, d2), ShapeMismatchError);
}

TEST(MatrixAnalysisTest, OpInferenceTranspose) {
    PropertyTable pt;
    pt.add_or_update_property_entry("U", {.shape = {3, 4}, .flags = {.is_upper_triangular = true}});
    EGraph g(pt);
    Id id_u = g.add_node(make_symbol("U"));
    Id id_tr = g.add_node(make_op(Op::Tr, {id_u}));

    const auto *prop = get_matrix_data(g, id_tr);
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->shape, (Shape{4, 3}));
    EXPECT_TRUE(prop->flags.is_lower_triangular);
    EXPECT_FALSE(prop->flags.is_upper_triangular);
}

TEST(MatrixAnalysisTest, OpInferenceMulDimensions) {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = {2, 5}});
    pt.add_or_update_property_entry("B", {.shape = {5, 3}});
    EGraph g(pt);
    Id id_a = g.add_node(make_symbol("A"));
    Id id_b = g.add_node(make_symbol("B"));
    Id id_ab = g.add_node(make_op(Op::Mul, {id_a, id_b}));

    const auto *prop = get_matrix_data(g, id_ab);
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->shape, (Shape{2, 3}));
}

TEST(MatrixAnalysisTest, ArityCheckThrowsAnalysisError) {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = {2, 2}});
    EGraph g(pt);
    Id id_a = g.add_node(make_symbol("A"));

    EXPECT_THROW(g.add_node(make_op(Op::Add, {id_a})), AnalysisError);
    EXPECT_THROW(g.add_node(make_op(Op::Tr, {id_a, id_a})), AnalysisError);
}

TEST(MatrixAnalysisTest, SymMulOnWideMatrixIsNotPositiveDefinite) {
    PropertyTable pt;
    pt.add_or_update_property_entry("W", {.shape = {2, 5}, .flags = {.is_full_rank = true}});
    EGraph g(pt);
    Id id_w = g.add_node(make_symbol("W"));
    Id id_sym = g.add_node(make_op(Op::SymMul, {id_w}));

    const auto *prop = get_matrix_data(g, id_sym);
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->shape, (Shape{5, 5}));
    EXPECT_TRUE(prop->flags.is_symmetric);
    EXPECT_FALSE(prop->flags.is_positive_definite);
    EXPECT_FALSE(prop->flags.is_non_singular);
    EXPECT_FALSE(prop->flags.is_full_rank);
}

TEST(MatrixAnalysisTest, SymMulOnTallMatrixWithoutFullRankIsNotPositiveDefinite) {
    PropertyTable pt;
    pt.add_or_update_property_entry("T", {.shape = {5, 2}, .flags = {.is_full_rank = false, .is_tall = true}});
    EGraph g(pt);
    Id id_t = g.add_node(make_symbol("T"));
    Id id_sym = g.add_node(make_op(Op::SymMul, {id_t}));

    const auto *prop = get_matrix_data(g, id_sym);
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->shape, (Shape{2, 2}));
    EXPECT_TRUE(prop->flags.is_symmetric);
    EXPECT_FALSE(prop->flags.is_positive_definite);
    EXPECT_FALSE(prop->flags.is_non_singular);
    EXPECT_FALSE(prop->flags.is_full_rank);
}

TEST(MatrixAnalysisTest, SyrkRankInferenceOnRectangularMatrices) {
    PropertyTable pt;
    pt.add_or_update_property_entry("Wide", {.shape = {2, 5}, .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("C5", {.shape = {5, 5}, .flags = {.is_zero = true}});
    pt.add_or_update_property_entry("Tall", {.shape = {5, 2}, .flags = {.is_full_rank = true}});
    EGraph g(pt);
    Id id_w = g.add_node(make_symbol("Wide"));
    Id id_c5 = g.add_node(make_symbol("C5"));
    Id id_t = g.add_node(make_symbol("Tall"));

    // Syrk_T(Wide, C5): Wide^T * Wide is 5x5, rank <= 2, singular!
    Id id_syrk_t = g.add_node(make_op(Op::Syrk_T, {id_w, id_c5}));
    const auto *prop_t = get_matrix_data(g, id_syrk_t);
    ASSERT_NE(prop_t, nullptr);
    EXPECT_EQ(prop_t->shape, (Shape{5, 5}));
    EXPECT_FALSE(prop_t->flags.is_positive_definite);
    EXPECT_FALSE(prop_t->flags.is_non_singular);
    EXPECT_FALSE(prop_t->flags.is_full_rank);

    // Syrk_N(Tall, C5): Tall * Tall^T is 5x5, rank <= 2, singular!
    Id id_syrk_n = g.add_node(make_op(Op::Syrk_N, {id_t, id_c5}));
    const auto *prop_n = get_matrix_data(g, id_syrk_n);
    ASSERT_NE(prop_n, nullptr);
    EXPECT_EQ(prop_n->shape, (Shape{5, 5}));
    EXPECT_FALSE(prop_n->flags.is_positive_definite);
    EXPECT_FALSE(prop_n->flags.is_non_singular);
    EXPECT_FALSE(prop_n->flags.is_full_rank);
}

TEST(MatrixAnalysisTest, AnalysisRejectsNonSquareFactorizationsAndInversion) {
    PropertyTable pt;
    pt.add_or_update_property_entry(
        "RectSym",
        {.shape = {"A", "B"}, .flags = {.is_symmetric = true, .is_positive_definite = true, .is_non_singular = true}});
    pt.add_or_update_property_entry(
        "RectNum",
        {.shape = {3, 4}, .flags = {.is_symmetric = true, .is_positive_definite = true, .is_non_singular = true}});
    EGraph g(pt);
    Id id_sym = g.add_node(make_symbol("RectSym"));
    Id id_num = g.add_node(make_symbol("RectNum"));

    EXPECT_THROW(g.add_node(make_op(Op::Inv, {id_sym})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::Inv, {id_num})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::CholeL, {id_sym})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::CholeL, {id_num})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::CholeU, {id_sym})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::CholeU, {id_num})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::Potrf_L, {id_sym})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::Potrf_L, {id_num})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::Trtri, {id_sym})), InvalidOperationError);
    EXPECT_THROW(g.add_node(make_op(Op::Trtri, {id_num})), InvalidOperationError);
}