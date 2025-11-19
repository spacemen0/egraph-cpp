#include "union_find.h"

Id UnionFind::find_root(Id current) const
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

Id UnionFind::find_and_compress(Id current)
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

void UnionFind::unite(Id x, Id y) noexcept
{
    Id rootX = find_and_compress(x);
    Id rootY = find_and_compress(y);
    if (rootX != rootY)
    {
        parents[rootY] = rootX;
    }
}

Id UnionFind::make_set()
{
    Id newIndex = parents.size();
    parents.push_back(newIndex);
    return newIndex;
}

size_t UnionFind::size() const noexcept
{
    return parents.size();
}
