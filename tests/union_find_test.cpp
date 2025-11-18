#include <gtest/gtest.h>
#include "UnionFind.h"

TEST(UnionFindRustTranslation, UnionFindGroupsAndCompression)
{
    const uint32_t n = 10;
    UnionFind uf;

    // create n singleton sets
    for (uint32_t i = 0; i < n; ++i)
    {
        uf.add();
    }

    EXPECT_EQ(uf.size(), n);

    // initially each element should be its own root
    for (uint32_t i = 0; i < n; ++i)
    {
        EXPECT_EQ(uf.find(i), i);
    }

    // build up one set: {0,1,2,3}
    uf.merge(0, 1);
    uf.merge(0, 2);
    uf.merge(0, 3);

    // build up another set: {6,7,8,9}
    uf.merge(6, 7);
    uf.merge(6, 8);
    uf.merge(6, 9);

    // perform finds that may trigger path compression
    for (uint32_t i = 0; i < n; ++i)
    {
        uf.find_mut(i);
    }

    // Verify membership:
    // 0,1,2,3 are in the same set
    for (uint32_t i = 1; i <= 3; ++i)
    {
        EXPECT_EQ(uf.find(i), uf.find(0));
    }

    // 4 and 5 remain singletons
    EXPECT_EQ(uf.find(4), 4u);
    EXPECT_EQ(uf.find(5), 5u);

    // 6,7,8,9 are in the same set
    for (uint32_t i = 7; i <= 9; ++i)
    {
        EXPECT_EQ(uf.find(i), uf.find(6));
    }
}