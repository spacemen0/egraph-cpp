#include <gtest/gtest.h>
#include "e_graph.h"
#include "test_helper.h"

// Global symbol constants
const ENode sym_a = make_symbol("A");
const ENode sym_b = make_symbol("B");
const ENode sym_c = make_symbol("C");
const ENode sym_d = make_symbol("D");
const ENode sym_x = make_symbol("X");
const ENode sym_y = make_symbol("Y");
const ENode sym_z = make_symbol("Z");
const ENode sym_w = make_symbol("W");

TEST(EGraph, AddAndLookUpNode)
{
    EGraph egraph;

    Id id1 = egraph.add_node(sym_x);
    std::optional<Id> lookup1 = egraph.find(sym_x);
    EXPECT_TRUE(lookup1.has_value());
    EXPECT_EQ(lookup1.value(), id1);

    Id id2 = egraph.add_node(sym_x);
    EXPECT_EQ(id1, id2);

    Id id3 = egraph.add_node(sym_y);
    EXPECT_NE(id1, id3);
}

TEST(EGraph, BreakWithInvalidChildId)
{
    EGraph egraph;
    Id fake_id = 999999;
    ENode dangerous_node = make_op(Op::Negate, Children{fake_id});
    EXPECT_ANY_THROW(egraph.add_node(dangerous_node));
}

TEST(EGraph, UnionClasses)
{
    EGraph egraph;

    Id id1 = egraph.add_node(sym_x);
    Id id2 = egraph.add_node(sym_y);

    bool merged = egraph.union_classes(id1, id2);
    EXPECT_TRUE(merged);

    merged = egraph.union_classes(id1, id2);
    EXPECT_FALSE(merged);
    egraph.rebuild();
    auto root1 = egraph.find(sym_x);
    auto root2 = egraph.find(sym_y);
    EXPECT_EQ(root1, root2);
}

TEST(EGraph, RebuildParentsBasic)
{
    EGraph egraph;

    auto id_node1 = egraph.add_expression(Expression("Add(A, B)"));
    auto id_node2 = egraph.add_expression(Expression("Add(A, C)"));

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));

    egraph.union_classes(egraph.find(sym_b).value(), egraph.find(sym_c).value());

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST(EGraph, RebuildParentsBasicReverse)
{
    EGraph egraph;

    auto id_node1 = egraph.add_expression(Expression("Add(A, B)"));
    auto id_node2 = egraph.add_expression(Expression("Add(A, C)"));

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.print_egraph();
    egraph.union_classes(egraph.find(sym_c).value(), egraph.find(sym_b).value());
    egraph.print_egraph();
    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();
    egraph.print_egraph();
    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST(EGraph, RebuildMultiLevelParents)
{
    EGraph egraph;

    Id id_add_ab = egraph.add_expression(Expression("Add(A, B)"));
    Id id_add_ac = egraph.add_expression(Expression("Add(A, C)"));
    EXPECT_NE(egraph.find_class_id(id_add_ab), egraph.find_class_id(id_add_ac));

    Id id_mul1 = egraph.add_expression(Expression("Mul(Add(A, B), D)"));
    Id id_mul2 = egraph.add_expression(Expression("Mul(Add(A, C), D)"));
    EXPECT_NE(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));

    Id id_nested1 = egraph.add_expression(Expression("Add(Mul(Add(A, B), D), A)"));
    Id id_nested2 = egraph.add_expression(Expression("Add(Mul(Add(A, C), D), A)"));
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    egraph.print_egraph();
    egraph.union_classes(egraph.find(sym_b).value(), egraph.find(sym_c).value());
    egraph.print_egraph();
    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));
    egraph.print_egraph();
    Id root_nested = egraph.find_class_id(id_nested1);
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested1));
    EXPECT_EQ(root_nested, egraph.find_class_id(id_nested2));
}

TEST(EGraph, AddExpression)
{
    EGraph egraph;

    Expression expr("Add(Mul(X, Y), Transpose(Z))");
    Id expr_id = egraph.add_expression(expr);

    EXPECT_TRUE(egraph.find(sym_x).has_value());
    EXPECT_TRUE(egraph.find(sym_y).has_value());
    EXPECT_TRUE(egraph.find(sym_z).has_value());

    auto mul_xy = make_op(Op::Mul, Children{egraph.find(sym_x).value(), egraph.find(sym_y).value()});
    auto transpose_z = make_op(Op::Transpose, Children{egraph.find(sym_z).value()});
    EXPECT_TRUE(egraph.find(mul_xy).has_value());
    EXPECT_TRUE(egraph.find(transpose_z).has_value());

    auto add_expr = make_op(Op::Add, Children{egraph.find(mul_xy).value(), egraph.find(transpose_z).value()});
    EXPECT_TRUE(egraph.find(add_expr).has_value());
    EXPECT_EQ(egraph.find(add_expr).value(), expr_id);

    Expression expr_dup("Add(Mul(X, Y), Transpose(Z))");
    Id expr_dup_id = egraph.add_expression(expr_dup);

    EXPECT_EQ(expr_id, expr_dup_id);

    Expression expr_diff("Add(Mul(X, Y), Invert(Z))");
    Id expr_diff_id = egraph.add_expression(expr_diff);

    EXPECT_NE(expr_id, expr_diff_id);
}

TEST(EGraph, ENodeMatching)
{
    EGraph egraph;

    Id id1 = egraph.add_node(sym_x);

    auto node = make_op(Op::Negate, Children{id1});
    Id id2 = egraph.add_node(node);

    EXPECT_TRUE(egraph.find(node).has_value());
    EXPECT_EQ(egraph.find(node).value(), id2);

    auto node_diff = make_op(Op::Negate, Children{id1 + 1});
    EXPECT_FALSE(egraph.find(node_diff).has_value());
}

TEST(EGraph, PatternMatchingSimple)
{
    EGraph egraph;

    Id expr_id = egraph.add_expression(Expression("Add(X, Y)"));

    Pattern pattern("Add(?a, ?b)");

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
    EGraph egraph;

    Id expr_id = egraph.add_expression(Expression("Add(Mul(X, Y), Transpose(Z))"));

    Pattern pattern("Add(Mul(?a, ?b), Transpose(?c))");

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
    EGraph egraph;

    Id id1 = egraph.add_expression(Expression("Add(X, Y)"));

    Id id2 = egraph.add_expression(Expression("Add(W, Z)"));

    egraph.union_classes(id1, id2);
    egraph.rebuild();

    Pattern pattern("Add(?a, ?b)");

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
        else if (atom_a == sym_w.get_atom() && atom_b == sym_z.get_atom())
        {
            found_wz = true;
        }
    }

    EXPECT_TRUE(found_xy);
    EXPECT_TRUE(found_wz);
}
