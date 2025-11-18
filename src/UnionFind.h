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

    uint32_t find(uint32_t current) const;

    uint32_t find_and_compress(uint32_t current);

    void merge(uint32_t x, uint32_t y) noexcept;
    uint32_t add();
    uint32_t size() const noexcept;

    // testing helper: return internal parent vector for exact-layout assertions
    const std::vector<uint32_t> &debug_parents() const noexcept { return parents; }

private:
    std::vector<uint32_t> parents;

#ifdef UNIT_TEST
    FRIEND_TEST(UnionFindParentsTest, ParentsLayoutAfterCompression);
#endif
};
