#include <gtest/gtest.h>
#include "e_graph.h"
#include "test_helper.h"
#include "errors.h"

TEST(EGraph, AddAndLookUpNode)
{
    EGraph egraph(get_property_table());

    Id id1 = egraph.add_node(sym_x);
    std::optional<Id> lookup1 = egraph.find_node_id(sym_x);
    EXPECT_TRUE(lookup1.has_value());
    EXPECT_EQ(lookup1.value(), id1);

    Id id2 = egraph.add_node(sym_x);
    EXPECT_EQ(id1, id2);

    Id id3 = egraph.add_node(sym_y);
    EXPECT_NE(id1, id3);
}

TEST(EGraph, BreakWithInvalidChildId)
{
    EGraph egraph(get_property_table());
    Id fake_id = 999999;
    ENode dangerous_node = make_op(Op::Neg, Children{fake_id});
    EXPECT_ANY_THROW(egraph.add_node(dangerous_node));
}

TEST(EGraph, UnionClasses)
{
    EGraph egraph(get_property_table());

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

TEST(EGraph, RebuildParentsBasic)
{
    EGraph egraph(get_property_table());

    auto id_node1 = egraph.add_expression(Expression("Add(D, D)"));
    auto id_node2 = egraph.add_expression(Expression("Add(D, W)"));

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));

    egraph.union_classes(egraph.find_node_id(sym_d).value(), egraph.find_node_id(sym_w).value());

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST(EGraph, RebuildMultiLevelParents)
{
    EGraph egraph(get_property_table());

    Id id_add_ab = egraph.add_expression(Expression("Add(A, Z)"));
    Id id_add_ac = egraph.add_expression(Expression("Add(A, A)"));
    EXPECT_NE(egraph.find_class_id(id_add_ab), egraph.find_class_id(id_add_ac));

    Id id_mul1 = egraph.add_expression(Expression("Mul(Add(A, Z), Z)"));
    Id id_mul2 = egraph.add_expression(Expression("Mul(Add(A, A), Z)"));
    EXPECT_NE(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));

    Id id_nested1 = egraph.add_expression(Expression("Add(Mul(Add(A, Z), Z), A)"));
    Id id_nested2 = egraph.add_expression(Expression("Add(Mul(Add(A, A), Z), A)"));
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    egraph.union_classes(egraph.find_node_id(sym_a).value(), egraph.find_node_id(sym_z).value());

    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    Id root_nested = egraph.find_class_id(id_nested1);
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested1));
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested2));
}

TEST(EGraph, RebuildCleanUpEClass)
{
    EGraph egraph(get_property_table());

    Id x = egraph.add_node(sym_a);
    Id y = egraph.add_node(sym_z);

    ENode node_x = make_op(Op::Neg, {x});
    Id neg_x = egraph.add_node(node_x);

    ENode node_y = make_op(Op::Neg, {y});
    Id neg_y = egraph.add_node(node_y);

    EXPECT_NE(egraph.find_class_id(neg_x), egraph.find_class_id(neg_y));

    egraph.union_classes(x, y);
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(neg_x), egraph.find_class_id(neg_y));

    EXPECT_EQ(egraph.get_class_nodes(egraph.find_class_id(neg_x)).size(), 1);
}

TEST(EGraph, AddExpression)
{
    EGraph egraph(get_property_table());

    Expression expr("Add(Mul(X, Y), Tr(Z))");
    Id expr_id = egraph.add_expression(expr);

    EXPECT_TRUE(egraph.find_node_id(sym_x).has_value());
    EXPECT_TRUE(egraph.find_node_id(sym_y).has_value());
    EXPECT_TRUE(egraph.find_node_id(sym_z).has_value());

    auto mul_xy = make_op(Op::Mul, Children{egraph.find_node_id(sym_x).value(), egraph.find_node_id(sym_y).value()});
    auto transpose_z = make_op(Op::Tr, Children{egraph.find_node_id(sym_z).value()});
    EXPECT_TRUE(egraph.find_node_id(mul_xy).has_value());
    EXPECT_TRUE(egraph.find_node_id(transpose_z).has_value());

    auto add_expr = make_op(Op::Add, Children{egraph.find_node_id(mul_xy).value(), egraph.find_node_id(transpose_z).value()});
    EXPECT_TRUE(egraph.find_node_id(add_expr).has_value());
    EXPECT_EQ(egraph.find_node_id(add_expr).value(), expr_id);

    Expression expr_dup("Add(Mul(X, Y), Tr(Z))");
    Id expr_dup_id = egraph.add_expression(expr_dup);

    EXPECT_EQ(expr_id, expr_dup_id);

    Expression expr_diff("Add(Mul(X, Y), Inv(Z))");
    Id expr_diff_id = egraph.add_expression(expr_diff);

    EXPECT_NE(expr_id, expr_diff_id);
}

TEST(EGraph, ENodeMatching)
{
    EGraph egraph(get_property_table());

    Id id1 = egraph.add_node(sym_x);

    auto node = make_op(Op::Neg, Children{id1});
    Id id2 = egraph.add_node(node);

    EXPECT_TRUE(egraph.find_node_id(node).has_value());
    EXPECT_EQ(egraph.find_node_id(node).value(), id2);

    auto node_diff = make_op(Op::Neg, Children{id1 + 1});
    EXPECT_FALSE(egraph.find_node_id(node_diff).has_value());
}

TEST(EGraph, PatternMatchingSimple)
{
    EGraph egraph(get_property_table());

    Id expr_id = egraph.add_expression(Expression("Mul(X, Y)"));

    Pattern pattern("Mul(?a, ?b)");

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(expr_id, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 1);
    const auto &substitution = *substitutions.begin();
    EXPECT_TRUE(substitution.contains("a"));
    EXPECT_TRUE(substitution.contains("b"));

    EXPECT_EQ(egraph.at(substitution.at("a")).get_atom(), sym_x.get_atom());
    EXPECT_EQ(egraph.at(substitution.at("b")).get_atom(), sym_y.get_atom());
}

TEST(EGraph, PatternMatchingNested)
{
    EGraph egraph(get_property_table());

    Id expr_id = egraph.add_expression(Expression("Add(Mul(X, Y), Tr(Z))"));

    Pattern pattern("Add(Mul(?a, ?b), Tr(?c))");

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(expr_id, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 1);
    const auto &substitution = *substitutions.begin();
    EXPECT_TRUE(substitution.contains("a"));
    EXPECT_TRUE(substitution.contains("b"));
    EXPECT_TRUE(substitution.contains("c"));

    EXPECT_EQ(egraph.at(substitution.at("a")).get_atom(), sym_x.get_atom());
    EXPECT_EQ(egraph.at(substitution.at("b")).get_atom(), sym_y.get_atom());
    EXPECT_EQ(egraph.at(substitution.at("c")).get_atom(), sym_z.get_atom());
}

TEST(EGraph, PatternMatchingMultipleMatchesInClass)
{
    EGraph egraph(get_property_table());

    Id id1 = egraph.add_expression(Expression("Mul(X, Y)"));

    Id id2 = egraph.add_expression(Expression("Mul(A, Z)"));

    egraph.union_classes(id1, id2);
    egraph.rebuild();

    Pattern pattern("Mul(?a, ?b)");

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(id1, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 2);

    bool found_xy = false;
    bool found_wz = false;

    for (const auto &sub : substitutions)
    {
        auto atom_a = egraph.at(sub.at("a")).get_atom();
        auto atom_b = egraph.at(sub.at("b")).get_atom();

        if (atom_a == sym_x.get_atom() && atom_b == sym_y.get_atom())
        {
            found_xy = true;
        }
        else if (atom_a == sym_a.get_atom() && atom_b == sym_z.get_atom())
        {
            found_wz = true;
        }
    }

    EXPECT_TRUE(found_xy);
    EXPECT_TRUE(found_wz);
}

TEST(EGraph, ErrorConditions)
{
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

TEST(EGraph, SampleSymbolicSizesIntoAnalysisData)
{
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

    egraph.sample_symbolic_sizes({{"A", 5}, {"B", 3}});

    const auto *m_prop_after = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_m).property);
    ASSERT_NE(m_prop_after, nullptr);
    EXPECT_EQ(m_prop_after->shape, std::make_pair(Size(5), Size(3)));

    const auto *mul_prop_after = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id_mul).property);
    ASSERT_NE(mul_prop_after, nullptr);
    EXPECT_EQ(mul_prop_after->shape, std::make_pair(Size(3), Size(3)));
}
