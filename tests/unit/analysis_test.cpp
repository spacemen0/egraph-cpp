#include "e_graph.h"
#include "errors.h"
#include "extractor.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

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