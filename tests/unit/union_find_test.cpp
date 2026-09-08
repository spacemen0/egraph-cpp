#include "union_find.h"
#include <gtest/gtest.h>
using namespace egraph;

TEST(UnionFind, ParentsLayoutAfterCompression) {
    UnionFind uf;
    const Id n = 10;
    for (Id i = 0; i < n; ++i)
        uf.make_set();

    // initial condition: each element is its own parent
    std::vector<Id> expected_init(n);
    for (Id i = 0; i < n; ++i)
        expected_init[i] = i;
    EXPECT_EQ(uf.get_parents(), expected_init);

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
    EXPECT_EQ(uf.get_parents(), expected);
}

TEST(UnionFind, MakeSetAndFindRoot) {
    UnionFind uf;
    Id id0 = uf.make_set();
    Id id1 = uf.make_set();
    Id id2 = uf.make_set();

    EXPECT_EQ(id0, 0);
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);

    EXPECT_EQ(uf.find_root(id0), id0);
    EXPECT_EQ(uf.find_root(id1), id1);
    EXPECT_EQ(uf.find_root(id2), id2);
}

TEST(UnionFind, UniteDisjointAndCheckRoot) {
    UnionFind uf;
    for (int i = 0; i < 5; ++i) {
        uf.make_set();
    }

    uf.unite(1, 2);
    EXPECT_EQ(uf.find_root(1), uf.find_root(2));
    EXPECT_NE(uf.find_root(0), uf.find_root(1));
    EXPECT_NE(uf.find_root(3), uf.find_root(1));

    uf.unite(3, 4);
    EXPECT_EQ(uf.find_root(3), uf.find_root(4));
    EXPECT_NE(uf.find_root(1), uf.find_root(3));

    uf.unite(2, 3);
    EXPECT_EQ(uf.find_root(1), uf.find_root(4));
}

TEST(UnionFind, UniteIdempotent) {
    UnionFind uf;
    uf.make_set();
    uf.make_set();

    uf.unite(0, 1);
    Id root_before = uf.find_root(0);
    uf.unite(0, 1);
    Id root_after = uf.find_root(0);

    EXPECT_EQ(root_before, root_after);
    EXPECT_EQ(uf.find_root(0), uf.find_root(1));
}

TEST(UnionFind, DeepChainPathCompression) {
    UnionFind uf;
    const int count = 8;
    for (int i = 0; i < count; ++i) {
        uf.make_set();
    }

    // Create linear chain: 0-1, 1-2, 2-3, 3-4, 4-5, 5-6, 6-7
    for (int i = 0; i < count - 1; ++i) {
        uf.unite(i, i + 1);
    }

    Id common_root = uf.find_root(0);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(uf.find_and_compress(i), common_root);
    }

    // After compressing all, every node's parent should directly be common_root
    for (Id parent : uf.get_parents()) {
        EXPECT_EQ(parent, common_root);
    }
}