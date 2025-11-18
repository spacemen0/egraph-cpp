#include "UnionFind.h"

uint32_t UnionFind::find(uint32_t current) const
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

uint32_t UnionFind::find_mut(uint32_t current)
{
    if (current >= parents.size())
    {
        throw std::out_of_range("Index out of range in UnionFind::find_mut");
    }
    if (parents[current] != current)
    {
        parents[current] = find_mut(parents[current]);
    }
    return parents[current];
}

void UnionFind::merge(uint32_t x, uint32_t y)
{
    uint32_t rootX = find_mut(x);
    uint32_t rootY = find_mut(y);
    if (rootX != rootY)
    {
        parents[rootY] = rootX;
    }
}

uint32_t UnionFind::add()
{
    uint32_t newIndex = parents.size();
    parents.push_back(newIndex);
    return newIndex;
}

uint32_t UnionFind::size() const
{
    return static_cast<uint32_t>(parents.size());
}
