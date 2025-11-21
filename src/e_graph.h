#pragma once
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>
#include "union_find.h"
#include "e_node.h"
#include "e_class.h"

class EGraph
{
public:
    // stores the union-find structure for e-classes (which stores equivalences)
    UnionFind uf;
    // stores all e-nodes in the order they were added
    std::vector<std::unique_ptr<ENode>> nodes;
    // stores pending parent updates after unions
    std::vector<Id> pendings;
    // stores mapping from ENode to EClass id
    std::unordered_map<const ENode *, Id, ENodePtrHash, ENodePtrEqual> memo;
    // stores mapping from EClass id to EClass
    std::unordered_map<Id, std::unique_ptr<EClass>> classes;

    std::optional<Id> look_up(ENode &node);
    Id add_node(ENode &node);
    bool union_classes(Id id1, Id id2);
    void rebuild();

private:
    Id add_class(const ENode &node, const ENode &original);
};