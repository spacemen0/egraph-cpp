#include <gtest/gtest.h>
#include "e_graph.h"
#include "test_helper.h"

TEST(EGraph, AddAndLookUpNode)
{
    EGraph egraph;

    auto sx = make_symbol("x");
    Id id1 = egraph.add_node(sx);
    std::optional<Id> lookup1 = egraph.find(sx);
    EXPECT_TRUE(lookup1.has_value());
    EXPECT_EQ(lookup1.value(), id1);

    Id id2 = egraph.add_node(sx);
    EXPECT_EQ(id1, id2);

    auto sy = make_symbol("y");
    Id id3 = egraph.add_node(sy);
    EXPECT_NE(id1, id3);
}

TEST(EGraph, UnionClasses)
{
    EGraph egraph;

    auto sx = make_symbol("x");
    Id id1 = egraph.add_node(sx);

    auto sy = make_symbol("y");
    Id id2 = egraph.add_node(sy);

    bool merged = egraph.union_classes(id1, id2);
    EXPECT_TRUE(merged);

    merged = egraph.union_classes(id1, id2);
    EXPECT_FALSE(merged);
    egraph.rebuild();
    auto root1 = egraph.find(sx);
    auto root2 = egraph.find(sy);
    EXPECT_EQ(root1, root2);
}

TEST(EGraph, RebuildParentsBasic)
{
    EGraph egraph;

    auto sa = make_symbol("a");
    auto sb = make_symbol("b");
    auto sc = make_symbol("c");
    Id ida = egraph.add_node(sa);
    Id idb = egraph.add_node(sb);
    Id idc = egraph.add_node(sc);

    auto node1 = make_op(Op::Add, Children{ida, idb});
    auto node2 = make_op(Op::Add, Children{ida, idc});

    auto id_node1 = egraph.add_node(node1);
    auto id_node2 = egraph.add_node(node2);

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));

    egraph.union_classes(idb, idc);

    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();

    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST(EGraph, RebuildParentsBasicReverse)
{
    EGraph egraph;

    auto sa = make_symbol("a");
    auto sb = make_symbol("b");
    auto sc = make_symbol("c");
    Id ida = egraph.add_node(sa);
    Id idb = egraph.add_node(sb);
    Id idc = egraph.add_node(sc);

    auto node1 = make_op(Op::Add, Children{ida, idb});
    auto node2 = make_op(Op::Add, Children{ida, idc});

    auto id_node1 = egraph.add_node(node1);
    auto id_node2 = egraph.add_node(node2);

    EXPECT_TRUE(egraph.find(node1).has_value());
    EXPECT_TRUE(egraph.find(node2).has_value());
    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.print_egraph();
    egraph.union_classes(idc, idb);
    egraph.print_egraph();
    EXPECT_NE(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
    egraph.rebuild();
    egraph.print_egraph();
    EXPECT_EQ(egraph.find_class_id(id_node1), egraph.find_class_id(id_node2));
}

TEST(EGraph, RebuildMultiLevelParents)
{
    EGraph egraph;

    auto a = make_symbol("a");
    auto b = make_symbol("b");
    auto c = make_symbol("c");
    auto d = make_symbol("d");

    Id ida = egraph.add_node(a);
    Id idb = egraph.add_node(b);
    Id idc = egraph.add_node(c);
    Id idd = egraph.add_node(d);

    auto add_ab = make_op(Op::Add, Children{ida, idb});
    auto add_ac = make_op(Op::Add, Children{ida, idc});

    Id id_add_ab = egraph.add_node(add_ab);
    Id id_add_ac = egraph.add_node(add_ac);
    EXPECT_NE(egraph.find_class_id(id_add_ab), egraph.find_class_id(id_add_ac));

    auto mul1 = make_op(Op::Mul, Children{id_add_ab, idd});
    auto mul2 = make_op(Op::Mul, Children{id_add_ac, idd});

    Id id_mul1 = egraph.add_node(mul1);
    Id id_mul2 = egraph.add_node(mul2);
    EXPECT_NE(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));

    auto nested1 = make_op(Op::Add, Children{id_mul1, ida});
    auto nested2 = make_op(Op::Add, Children{id_mul2, ida});

    Id id_nested1 = egraph.add_node(nested1);
    Id id_nested2 = egraph.add_node(nested2);
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));

    egraph.print_egraph();
    egraph.union_classes(idb, idc);
    egraph.print_egraph();
    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id_add_ab), egraph.find_class_id(id_add_ac));
    EXPECT_NE(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));
    egraph.print_egraph();
    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id_mul1), egraph.find_class_id(id_mul2));
    EXPECT_NE(egraph.find_class_id(id_nested1), egraph.find_class_id(id_nested2));
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

    Expression expr("Add(Mul(x, y), Transpose(z))");
    Id expr_id = egraph.add_expression(expr);

    auto x = make_symbol("x");
    auto y = make_symbol("y");
    auto z = make_symbol("z");
    EXPECT_TRUE(egraph.find(x).has_value());
    EXPECT_TRUE(egraph.find(y).has_value());
    EXPECT_TRUE(egraph.find(z).has_value());

    auto mul_xy = make_op(Op::Mul, Children{egraph.find(x).value(), egraph.find(y).value()});
    auto transpose_z = make_op(Op::Transpose, Children{egraph.find(z).value()});
    EXPECT_TRUE(egraph.find(mul_xy).has_value());
    EXPECT_TRUE(egraph.find(transpose_z).has_value());

    auto add_expr = make_op(Op::Add, Children{egraph.find(mul_xy).value(), egraph.find(transpose_z).value()});
    EXPECT_TRUE(egraph.find(add_expr).has_value());
    EXPECT_EQ(egraph.find(add_expr).value(), expr_id);

    Expression expr_dup("Add(Mul(x, y), Transpose(z))");
    Id expr_dup_id = egraph.add_expression(expr_dup);

    EXPECT_EQ(expr_id, expr_dup_id);

    Expression expr_diff("Add(Mul(x, y), Invert(z))");
    Id expr_diff_id = egraph.add_expression(expr_diff);

    EXPECT_NE(expr_id, expr_diff_id);
}

TEST(EGraph, ComplexExpressionEquivalence)
{
    EGraph egraph;

    Expression expr1("Add(Mul(a, b), Mul(c, d))");
    Expression expr2("Add(Mul(c, d), Mul(a, b))");

    Id id1 = egraph.add_expression(expr1);
    Id id2 = egraph.add_expression(expr2);

    EXPECT_NE(id1, id2);

    auto mul_ab = egraph.find(make_op(Op::Mul, {egraph.find(make_symbol("a")).value(),
                                                egraph.find(make_symbol("b")).value()}))
                      .value();
    auto mul_cd = egraph.find(make_op(Op::Mul, {egraph.find(make_symbol("c")).value(),
                                                egraph.find(make_symbol("d")).value()}))
                      .value();

    auto add1 = make_op(Op::Add, Children{mul_ab, mul_cd});
    auto add2 = make_op(Op::Add, Children{mul_cd, mul_ab});

    EXPECT_NE(egraph.find(add1).value(), egraph.find(add2).value());

    egraph.union_classes(egraph.find_class_id(mul_ab), egraph.find_class_id(mul_cd));
    egraph.rebuild();
    EXPECT_EQ(egraph.find_class_id(id1), egraph.find_class_id(id2));
}

TEST(EGraph, ENodeMatching)
{
    EGraph egraph;

    auto x = make_symbol("x");
    Id id1 = egraph.add_node(x);

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

    Expression expr("Add(x, y)");
    Id expr_id = egraph.add_expression(expr);

    Pattern pattern{
        .atom = Op::Add,
        .children = {
            {.atom = PatternVar{"?a"}},
            {.atom = PatternVar{"?b"}},

        },
    };

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(expr_id, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 1);
    const auto &substitution = *substitutions.begin();
    EXPECT_TRUE(substitution.contains("?a"));
    EXPECT_TRUE(substitution.contains("?b"));

    EXPECT_EQ(egraph.at(substitution.at("?a")).get_atom(), make_symbol("x").get_atom());
    EXPECT_EQ(egraph.at(substitution.at("?b")).get_atom(), make_symbol("y").get_atom());
}

TEST(EGraph, PatternMatchingNested)
{
    EGraph egraph;

    Expression expr("Add(Mul(x, y), Transpose(z))");
    Id expr_id = egraph.add_expression(expr);

    Pattern pattern{
        .atom = Op::Add,
        .children = {
            {
                .atom = Op::Mul,
                .children = {
                    {.atom = PatternVar{"?a"}},
                    {.atom = PatternVar{"?b"}},
                },
            },
            {
                .atom = Op::Transpose,
                .children = {
                    {.atom = PatternVar{"?c"}},
                },
            },
        },
    };

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(expr_id, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 1);
    const auto &substitution = *substitutions.begin();
    EXPECT_TRUE(substitution.contains("?a"));
    EXPECT_TRUE(substitution.contains("?b"));
    EXPECT_TRUE(substitution.contains("?c"));

    EXPECT_EQ(egraph.at(substitution.at("?a")).get_atom(), make_symbol("x").get_atom());
    EXPECT_EQ(egraph.at(substitution.at("?b")).get_atom(), make_symbol("y").get_atom());
    EXPECT_EQ(egraph.at(substitution.at("?c")).get_atom(), make_symbol("z").get_atom());
}

TEST(EGraph, PatternMatchingMultipleMatchesInClass)
{
    EGraph egraph;

    Expression expr1("Add(x, y)");
    Id id1 = egraph.add_expression(expr1);

    Expression expr2("Add(w, z)");
    Id id2 = egraph.add_expression(expr2);

    egraph.union_classes(id1, id2);
    egraph.rebuild();

    Pattern pattern{
        .atom = Op::Add,
        .children = {
            {.atom = PatternVar{"?a"}},
            {.atom = PatternVar{"?b"}},
        },
    };

    std::set<Substitution> substitutions;
    egraph.find_matches_in_eclass(id1, pattern, substitutions);

    EXPECT_EQ(substitutions.size(), 2);

    bool found_xy = false;
    bool found_wz = false;

    for (const auto &sub : substitutions)
    {
        auto atom_a = egraph.at(sub.at("?a")).get_atom();
        auto atom_b = egraph.at(sub.at("?b")).get_atom();

        if (atom_a == make_symbol("x").get_atom() && atom_b == make_symbol("y").get_atom())
        {
            found_xy = true;
        }
        else if (atom_a == make_symbol("w").get_atom() && atom_b == make_symbol("z").get_atom())
        {
            found_wz = true;
        }
    }

    EXPECT_TRUE(found_xy);
    EXPECT_TRUE(found_wz);
}
