#include "union_find.h"

uint32_t UnionFind::find_root(uint32_t current) const
{
    if (current >= parents.size())
    {
        throw std::out_of_range("Index out of range in UnionFind::find");
    }
    while (parents[current] != current)
    {
        current = parents[current];
    }
    return current;
}

uint32_t UnionFind::find_and_compress(uint32_t current)
{
    if (current >= parents.size())
    {
        throw std::out_of_range("Index out of range in UnionFind::find_mut");
    }
    if (parents[current] != current)
    {
        parents[current] = find_and_compress(parents[current]);
    }
    return parents[current];
}

void UnionFind::unite(uint32_t x, uint32_t y) noexcept
{
    uint32_t rootX = find_and_compress(x);
    uint32_t rootY = find_and_compress(y);
    if (rootX != rootY)
    {
        parents[rootY] = rootX;
    }
}

uint32_t UnionFind::make_set()
{
    uint32_t newIndex = parents.size();
    parents.push_back(newIndex);
    return newIndex;
}

size_t UnionFind::size() const noexcept
{
    return parents.size();
}
