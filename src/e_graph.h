#pragma once
#include <vector>
#include <optional>
#include <unordered_map>
#include "union_find.h"
#include "e_node.h"
#include "e_class.h"

class EGraph
{
public:
    // stores the union-find structure for e-classes (which stores equivalences)
    UnionFind uf;
    // stores all e-nodes (Ids are the initial indices when added)
    std::vector<ENode> nodes;
    std::vector<Id> pendings;
    // stores mapping from ENode to EClass id
    std::unordered_map<ENode, Id> memo;
    // stores mapping from EClass id to EClass
    std::unordered_map<Id, EClass> classes;

    std::optional<Id> look_up(ENode &node);
    Id add_node(ENode &node);
    bool union_classes(Id id1, Id id2);
    void rebuild();

private:
    Id add_class(const ENode &node, const ENode &original);
};