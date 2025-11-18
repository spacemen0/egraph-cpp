#pragma once
#include <cstdint>
#include <vector>

#ifdef UNIT_TEST
#include <gtest/gtest.h>
#endif

class UnionFind
{
public:
    UnionFind() = default;

    uint32_t find_root(uint32_t current) const;

    uint32_t find_and_compress(uint32_t current);

    void unite(uint32_t x, uint32_t y) noexcept;
    uint32_t make_set();
    size_t size() const noexcept;

private:
    std::vector<uint32_t> parents;

#ifdef UNIT_TEST
    FRIEND_TEST(UnionFind, ParentsLayoutAfterCompression);
#endif
};
