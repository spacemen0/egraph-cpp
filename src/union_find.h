#pragma once
#include <cstdint>
#include <vector>

#ifdef UNIT_TEST
#include <gtest/gtest.h>
#endif
#include "types.h"

class UnionFind
{
public:
    UnionFind() = default;

    Id find_root(Id current) const;

    Id find_and_compress(Id current);

    void unite(Id x, Id y) noexcept;
    Id make_set();
    size_t size() const noexcept;

private:
    std::vector<Id> parents;
#ifdef UNIT_TEST
    FRIEND_TEST(UnionFind, ParentsLayoutAfterCompression);
#endif
};
