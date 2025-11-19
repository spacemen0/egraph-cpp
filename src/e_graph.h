#pragma once
#include "union_find.h"
#include <vector>
#include <unordered_map>

class EGraph
{
public:
    UnionFind uf;
    std::vector<int> nodes;
    std::unordered_map<int, int> node_to_id;
};