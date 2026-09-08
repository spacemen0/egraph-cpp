#include "e_class.h"
#include "e_node.h"
#include <gtest/gtest.h>
using namespace egraph;

using enum Op;

TEST(EClass, CleanUpNodes) {
    Children c12 = {1, 2};
    Children c34 = {3, 4};

    ENode nodeA(c12, Add);
    ENode nodeB(c12, Add);
    ENode nodeC(c12, Mul);
    ENode nodeD(c34, Add);
    ENode nodeE(c12, Add);

    AnalysisData data;
    EClass eclass(0, &nodeA, data);

    eclass.get_nodes().push_back(&nodeB);
    eclass.get_nodes().push_back(&nodeC);
    eclass.get_nodes().push_back(&nodeD);
    eclass.get_nodes().push_back(&nodeE);

    EXPECT_EQ(eclass.get_nodes().size(), 5);

    eclass.clean_up_nodes();

    const auto &nodes = eclass.get_nodes();
    EXPECT_EQ(nodes.size(), 3);

    int countAdd12 = 0;
    int countMul12 = 0;
    int countAdd34 = 0;

    for (const auto *node : nodes) {
        if (*node == nodeA)
            countAdd12++;
        else if (*node == nodeC)
            countMul12++;
        else if (*node == nodeD)
            countAdd34++;
    }

    EXPECT_EQ(countAdd12, 1);
    EXPECT_EQ(countMul12, 1);
    EXPECT_EQ(countAdd34, 1);
}

TEST(EClass, ConstructorAndGetters) {
    ENode nodeA({1, 2}, Add);
    MatrixProperty prop{.shape = {2, 2}, .flags = {.is_identity = true}};
    AnalysisData data{prop};

    EClass eclass(42, &nodeA, data);

    EXPECT_EQ(eclass.get_nodes().size(), 1);
    EXPECT_EQ(eclass.get_nodes()[0], &nodeA);
    EXPECT_TRUE(eclass.get_parents().empty());

    auto *retrieved_prop = std::get_if<MatrixProperty>(&eclass.get_analysis_data().property);
    ASSERT_NE(retrieved_prop, nullptr);
    EXPECT_TRUE(retrieved_prop->flags.is_identity);
}

TEST(EClass, ParentsTracking) {
    ENode nodeA({1, 2}, Add);
    AnalysisData data;
    EClass eclass(1, &nodeA, data);

    eclass.get_parents().push_back(10);
    eclass.get_parents().push_back(20);

    ASSERT_EQ(eclass.get_parents().size(), 2);
    EXPECT_EQ(eclass.get_parents()[0], 10);
    EXPECT_EQ(eclass.get_parents()[1], 20);
}

TEST(EClass, AnalysisDataMutation) {
    ENode nodeA({1, 2}, Add);
    AnalysisData data;
    EClass eclass(1, &nodeA, data);

    MatrixProperty new_prop{.shape = {3, 3}, .flags = {.is_symmetric = true}};
    eclass.get_analysis_data() = AnalysisData{new_prop};

    auto *retrieved_prop = std::get_if<MatrixProperty>(&eclass.get_analysis_data().property);
    ASSERT_NE(retrieved_prop, nullptr);
    EXPECT_TRUE(retrieved_prop->flags.is_symmetric);
    EXPECT_EQ(retrieved_prop->shape, (Shape{3, 3}));
}

