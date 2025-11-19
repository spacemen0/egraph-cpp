#pragma once
#include "union_find.h"
#include <vector>
#include <unordered_map>
#include "e_node.h"
#include "e_class.h"

class EGraph
{
public:
    // stores the union-find structure for e-classes (which stores equivalences)
    UnionFind uf;
    // stores all e-nodes
    std::vector<Id> nodes;
    // stores mapping from ENode to EClass id
    std::unordered_map<ENode, Id> memo;
    // stores mapping from EClass id to EClass
    std::unordered_map<Id, EClass> classes;
};