#include "e_class.h"
#include "e_node.h"
#include <gtest/gtest.h>

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
