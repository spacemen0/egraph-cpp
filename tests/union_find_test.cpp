#include <gtest/gtest.h>
#include "union_find.h"
#include <vector>

TEST(UnionFind, ParentsLayoutAfterCompression)
{
    UnionFind uf;
    const Id n = 10;
    for (Id i = 0; i < n; ++i)
        uf.make_set();

    // initial condition: each element is its own parent
    std::vector<Id> expected_init(n);
    for (Id i = 0; i < n; ++i)
        expected_init[i] = i;
    EXPECT_EQ(uf.parents, expected_init);

    // build up one set: {0,1,2,3}
    uf.unite(0, 1);
    uf.unite(0, 2);
    uf.unite(0, 3);

    // build up another set: {6,7,8,9}
    uf.unite(6, 7);
    uf.unite(6, 8);
    uf.unite(6, 9);

    // compress paths
    for (Id i = 0; i < n; ++i)
        uf.find_and_compress(i);

    // expected parents after compression
    std::vector<Id> expected = {0, 0, 0, 0, 4, 5, 6, 6, 6, 6};
    EXPECT_EQ(uf.parents, expected);
}