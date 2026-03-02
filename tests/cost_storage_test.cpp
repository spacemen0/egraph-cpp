#include <gtest/gtest.h>
#include "cost_storage.h"
#include "e_graph.h"
#include "test_helper.h"
#include <limits>

TEST(CostStorageTest, BasicCost)
{
    EGraph egraph(get_property_table());

    Id id_a = egraph.add_node(sym_a);
    Id id_z = egraph.add_node(sym_z);

    ENode add_node = make_op(Op::Add, {id_a, id_z});
    Id id_add = egraph.add_node(add_node);

    ENode mul_node = make_op(Op::Mul, {id_add, id_a});
    Id id_mul = egraph.add_node(mul_node);

    auto &costs = egraph.get_cost_storage();

    egraph.update_cost_storage();
    EXPECT_EQ(costs.eclass_cost(id_a), 1.0);
    EXPECT_EQ(costs.eclass_cost(id_z), 1.0);

    EXPECT_EQ(costs.eclass_cost(id_add), 3.0);
}

TEST(CostStorageTest, CycleHandling)
{
    EGraph egraph(get_property_table());

    Id id_a = egraph.add_node(sym_a);
    Id id_z = egraph.add_node(sym_z);

    ENode add_az = make_op(Op::Add, {id_a, id_z});
    Id id_add = egraph.add_node(add_az);

    egraph.union_classes(id_a, id_add);
    egraph.rebuild();

    auto &costs = egraph.get_cost_storage();
    egraph.update_cost_storage();
    EXPECT_EQ(costs.eclass_cost(id_add), 1.0);

    const ENode *best = costs.best_node(id_add);
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(*best, sym_a);
}

TEST(CostStorageTest, BestNodeSelection)
{
    EGraph egraph(get_property_table());

    Id id_a = egraph.add_node(sym_a);

    ENode add_node = make_op(Op::Add, {id_a, id_a});
    Id id_add = egraph.add_node(add_node);

    egraph.union_classes(id_a, id_add);
    egraph.rebuild();

    auto &costs = egraph.get_cost_storage();
    egraph.update_cost_storage();
    const ENode *best = costs.best_node(id_add);
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(*best, sym_a);
    EXPECT_EQ(costs.eclass_cost(id_add), 1.0);
    EXPECT_TRUE(costs.has_finite_cost(id_add));
}

TEST(CostStorageTest, RecalculationAndPropagation)
{
    EGraph egraph(get_property_table());

    Id id_a = egraph.add_node(sym_a); // cost 1
    Id id_z = egraph.add_node(sym_z); // cost 1

    ENode add1 = make_op(Op::Add, {id_a, id_z});
    Id id_add1 = egraph.add_node(add1); // cost 3

    ENode add2 = make_op(Op::Add, {id_add1, id_z});
    Id id_add2 = egraph.add_node(add2); // cost 5

    ENode add3 = make_op(Op::Add, {id_add2, id_z});
    Id id_add3 = egraph.add_node(add3); // cost 7

    auto &costs = egraph.get_cost_storage();
    egraph.update_cost_storage();
    EXPECT_EQ(costs.eclass_cost(id_add3), 7.0);

    egraph.union_classes(id_add2, id_a);
    egraph.rebuild();

    egraph.update_cost_storage();
    EXPECT_EQ(costs.eclass_cost(id_add2), 1.0);
    EXPECT_EQ(costs.eclass_cost(id_add3), 3.0);
}
