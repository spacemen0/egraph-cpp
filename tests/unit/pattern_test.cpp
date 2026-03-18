#include <gtest/gtest.h>
#include "e_graph.h"
#include "test_helpers.h"

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