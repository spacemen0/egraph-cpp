#pragma once
#include <vector>
#include <optional>
#include <memory>
#include <unordered_map>
#include "union_find.h"
#include "e_node.h"
#include "e_class.h"
#include "expression.h"

class EGraph
{
public:
    std::optional<Id> find_and_canonicalize(ENode &node);
    std::optional<Id> find(const ENode &node);
    Id find_class_id(Id node_id) const;
    Id add_node(ENode node);
    Id add_expression(const Expression &expr);
    bool union_classes(Id id1, Id id2);
    void rebuild();
    const ENode &at(Id id) const;

private:
    // stores the union-find structure for e-classes (which stores equivalences)
    UnionFind uf;
    // stores all e-nodes in the order they were added
    std::vector<std::unique_ptr<ENode>> nodes;
    // stores pending parent updates after unions
    std::vector<Id> pendings;
    // stores mapping from ENode to EClass id (canonicalized one after rebuild)
    std::unordered_map<const ENode *, Id, ENodePtrHash, ENodePtrEqual> memo;
    // stores mapping from EClass id to EClass
    std::unordered_map<Id, std::unique_ptr<EClass>> classes;
};