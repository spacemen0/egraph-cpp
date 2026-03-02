#include "cost_storage.h"

#include "e_graph.h"

#include <limits>

CostStorage::CostStorage(const EGraph &egraph) : egraph(egraph)
{
    compute();
}

double CostStorage::node_cost(const ENode &node) const
{
    auto cached = node_costs.find(&node);
    if (cached != node_costs.end())
    {
        return cached->second;
    }

    double cost = 1.0;
    for (Id child : node.get_children())
    {
        Id root = egraph.find_class_id(child);
        auto it = e_class_costs.find(root);
        if (it == e_class_costs.end() || it->second == std::numeric_limits<double>::infinity())
        {
            return std::numeric_limits<double>::infinity();
        }
        cost += it->second;
    }
    return cost;
}

void CostStorage::compute()
{
    e_class_costs.clear();
    best_nodes_in_e_class.clear();
    node_costs.clear();

    for (Id id : egraph.get_all_class_ids())
    {
        Id root = egraph.find_class_id(id);
        e_class_costs[root] = std::numeric_limits<double>::infinity();
    }

    bool changed = true;
    while (changed)
    {
        changed = false;

        for (Id class_id : egraph.get_all_class_ids())
        {
            Id root = egraph.find_class_id(class_id);
            if (root != class_id)
                continue;

            for (const ENode *node : egraph.get_class_nodes(root))
            {
                double current_node_cost = node->compute_cost(egraph);
                for (Id child : node->get_children())
                {
                    Id child_root = egraph.find_class_id(child);
                    auto child_cost_it = e_class_costs.find(child_root);
                    if (child_cost_it == e_class_costs.end() || child_cost_it->second == std::numeric_limits<double>::infinity())
                    {
                        current_node_cost = std::numeric_limits<double>::infinity();
                        break;
                    }
                    current_node_cost += child_cost_it->second;
                }

                node_costs[node] = current_node_cost;

                if (current_node_cost >= e_class_costs[root])
                    continue;

                e_class_costs[root] = current_node_cost;
                best_nodes_in_e_class[root] = node;
                changed = true;
            }
        }
    }
}

double CostStorage::eclass_cost(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    auto it = e_class_costs.find(root);
    if (it == e_class_costs.end())
    {
        return std::numeric_limits<double>::infinity();
    }
    return it->second;
}

bool CostStorage::has_finite_cost(Id class_id) const
{
    return eclass_cost(class_id) != std::numeric_limits<double>::infinity();
}

const ENode *CostStorage::best_node(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    auto it = best_nodes_in_e_class.find(root);
    if (it == best_nodes_in_e_class.end())
    {
        return nullptr;
    }
    return it->second;
}
