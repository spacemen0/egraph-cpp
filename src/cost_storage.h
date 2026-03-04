#pragma once

#include "e_node.h"
#include "types.h"
#include <unordered_map>

class EGraph;

class CostStorage
{
public:
    void compute();
    Cost eclass_cost(Id class_id) const;
    Cost node_cost(const ENode *node);
    bool has_finite_cost(Id class_id) const;
    const ENode *best_node(Id class_id) const;

private:
    explicit CostStorage(const EGraph &egraph);
    const EGraph &egraph;
    std::unordered_map<Id, Cost> e_class_costs;
    std::unordered_map<Id, const ENode *> best_nodes_in_e_class;
    std::unordered_map<const ENode *, Cost, ENodePtrHash, ENodePtrEqual> node_costs;

    friend class EGraph;
};
