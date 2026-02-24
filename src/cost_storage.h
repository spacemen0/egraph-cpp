#pragma once

#include "e_node.h"

#include <unordered_map>

class EGraph;

class CostStorage
{
public:
    explicit CostStorage(const EGraph &egraph);

    void recompute();
    double eclass_cost(Id class_id) const;
    double node_cost(const ENode &node) const;
    bool has_finite_cost(Id class_id) const;
    const ENode *best_node(Id class_id) const;

private:
    const EGraph &egraph;
    std::unordered_map<Id, double> e_class_costs;
    std::unordered_map<Id, const ENode *> best_nodes_in_e_class;
    std::unordered_map<const ENode *, double> node_costs;
};
