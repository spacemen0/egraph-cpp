#include <gtest/gtest.h>
#include "UnionFind.h"
#include <vector>

TEST(UnionFindParentsTest, ParentsLayoutAfterCompression)
{
    UnionFind uf;
    const uint32_t n = 10;
    for (uint32_t i = 0; i < n; ++i)
        uf.add();

    // initial condition: each element is its own parent
    std::vector<uint32_t> expected_init(n);
    for (uint32_t i = 0; i < n; ++i)
        expected_init[i] = i;
    EXPECT_EQ(uf.parents, expected_init);

    // build up one set: {0,1,2,3}
    uf.merge(0, 1);
    uf.merge(0, 2);
    uf.merge(0, 3);

    // build up another set: {6,7,8,9}
    uf.merge(6, 7);
    uf.merge(6, 8);
    uf.merge(6, 9);

    // compress paths
    for (uint32_t i = 0; i < n; ++i)
        uf.find_and_compress(i);

    // expected parents after compression
    std::vector<uint32_t> expected = {0, 0, 0, 0, 4, 5, 6, 6, 6, 6};
    EXPECT_EQ(uf.parents, expected);
}