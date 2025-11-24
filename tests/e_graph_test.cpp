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

TEST(EGraph, RebuildPendingParentsBasic)
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

    Id add1 = egraph.add_node(node1);
    Id add2 = egraph.add_node(node2);

    EXPECT_NE(egraph.find(node1).value(), egraph.find(node2).value());

    egraph.union_classes(idb, idc);

    EXPECT_NE(egraph.find(node1).value(), egraph.find(node2).value());

    egraph.rebuild();

    EXPECT_EQ(egraph.find(node1).value(), egraph.find(node2).value());
}

TEST(EGraph, RebuildMultiLevelPendings)
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
    EXPECT_NE(egraph.find(add_ab).value(), egraph.find(add_ac).value());

    auto mul1 = make_op(Op::Mul, Children{id_add_ab, idd});
    auto mul2 = make_op(Op::Mul, Children{id_add_ac, idd});

    Id id_mul1 = egraph.add_node(mul1);
    Id id_mul2 = egraph.add_node(mul2);
    EXPECT_NE(egraph.find(mul1).value(), egraph.find(mul2).value());

    auto nested1 = make_op(Op::Add, Children{id_mul1, ida});
    auto nested2 = make_op(Op::Add, Children{id_mul2, ida});

    Id id_nested1 = egraph.add_node(nested1);
    Id id_nested2 = egraph.add_node(nested2);
    EXPECT_NE(egraph.find(nested1).value(), egraph.find(nested2).value());

    egraph.union_classes(idb, idc);

    egraph.rebuild();
    EXPECT_EQ(egraph.find(add_ab).value(), egraph.find(add_ac).value());
    EXPECT_NE(egraph.find(mul1).value(), egraph.find(mul2).value());
    EXPECT_NE(egraph.find(nested1).value(), egraph.find(nested2).value());

    egraph.rebuild();
    EXPECT_EQ(egraph.find(mul1).value(), egraph.find(mul2).value());
    EXPECT_NE(egraph.find(nested1).value(), egraph.find(nested2).value());

    egraph.rebuild();
    EXPECT_EQ(egraph.find(nested1).value(), egraph.find(nested2).value());

    auto root_nested = egraph.find(nested1).value();
    egraph.rebuild();
    EXPECT_EQ(root_nested, egraph.find(nested1).value());
    EXPECT_EQ(root_nested, egraph.find(nested2).value());
}
