#include "extractor.h"
#include <limits>
#include <iostream>

Extractor::Extractor(const EGraph &egraph) : egraph(egraph)
{
    calculate_costs();
}

double Extractor::get_node_cost(const ENode &node) const
{
    // Simple cost function
    double cost = 1.0;
    for (Id child : node.get_children())
    {
        Id root = egraph.find_class_id(child);
        if (costs.find(root) == costs.end())
        {
            return std::numeric_limits<double>::infinity();
        }
        cost += costs.at(root);
    }
    return cost;
}

void Extractor::calculate_costs()
{

    for (Id id : egraph.get_all_class_ids())
    {
        costs[id] = std::numeric_limits<double>::infinity();
    }

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (Id class_id : egraph.get_all_class_ids())
        {
            // Only process root classes
            Id root = egraph.find_class_id(class_id);
            if (root != class_id)
                continue;

            for (const ENode *node : egraph.get_class_nodes(root))
            {
                double node_cost = get_node_cost(*node);
                if (node_cost >= costs[root])
                    continue;
                costs[root] = node_cost;
                best_nodes[root] = node;
                changed = true;
            }
        }
    }
}

Expression Extractor::build_expression(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    const ENode *best_node = best_nodes.at(root);

    std::vector<Expression> children;
    for (Id child : best_node->get_children())
    {
        children.push_back(build_expression(child));
    }
    return Expression(best_node->get_atom(), children);
}

ExtractionResult Extractor::extract(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    if (costs.at(root) == std::numeric_limits<double>::infinity())
    {
        throw std::runtime_error("Cannot extract from class with infinite cost");
    }
    return {costs.at(root), build_expression(root)};
}
