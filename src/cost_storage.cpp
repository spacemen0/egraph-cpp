#include "cost_storage.h"
#include "e_graph.h"
#include <limits>
#include <cmath>

CostStorage::CostStorage(const EGraph &egraph) : egraph(egraph)
{
}

void CostStorage::ensure_root_cost_cache_fresh() const
{
    uint64_t revision = egraph.get_revision();
    if (root_cost_cache_revision != revision)
    {
        root_extraction_cache.clear();
        root_cost_cache_revision = revision;
    }
}

Cost CostStorage::node_cost(const ENode *node)
{
    auto cached = node_costs.find(node);
    if (cached != node_costs.end())
    {
        return cached->second;
    }

    Cost cost = node->compute_local_cost(egraph);

    for (Id child : node->get_children())
    {
        Id root = egraph.find_class_id(child);
        auto it = e_class_costs.find(root);
        if (it == e_class_costs.end() || std::holds_alternative<double>(it->second) && std::get<double>(it->second) == std::numeric_limits<double>::infinity())
        {
            cost = std::numeric_limits<double>::infinity();
            break;
        }
        cost += it->second;
    }
    node_costs.emplace(node, cost);
    return cost;
}

void CostStorage::compute()
{
    e_class_costs.clear();
    best_nodes_in_e_class.clear();

    for (Id id : egraph.get_all_class_ids())
    {
        Id root = egraph.find_class_id(id);
        e_class_costs[root] = std::numeric_limits<double>::infinity();
    }

    bool changed = true;
    while (changed)
    {
        node_costs.clear();
        changed = false;
        for (Id class_id : egraph.get_all_class_ids())
        {
            Id root = egraph.find_class_id(class_id);
            if (root != class_id)
                continue;
            for (const ENode *node : egraph.get_class_nodes(root))
            {
                Cost current_node_cost = node_cost(node);

                if (std::holds_alternative<double>(current_node_cost) &&
                    std::holds_alternative<double>(e_class_costs[root]))
                {
                    if (std::get<double>(current_node_cost) >= std::get<double>(e_class_costs[root]))
                    {
                        continue;
                    }
                }
                else
                {
                    continue;
                }

                e_class_costs[root] = current_node_cost;
                best_nodes_in_e_class[root] = node;
                changed = true;
            }
        }
    }
}

Cost CostStorage::eclass_cost(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    auto it = e_class_costs.find(root);
    if (it == e_class_costs.end())
    {
        return std::numeric_limits<double>::infinity();
    }
    return it->second;
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

std::optional<CostStorage::CachedRootExtraction> CostStorage::cached_root_extraction(Id class_id) const
{
    if (!egraph.is_clean())
    {
        return std::nullopt;
    }
    ensure_root_cost_cache_fresh();

    Id root = egraph.find_class_id(class_id);
    auto it = root_extraction_cache.find(root);
    if (it == root_extraction_cache.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void CostStorage::store_root_extraction(Id class_id, double cost, const std::unordered_map<Id, const ENode *> &choices) const
{
    if (!egraph.is_clean() || !std::isfinite(cost) || choices.empty())
    {
        return;
    }
    ensure_root_cost_cache_fresh();

    Id root = egraph.find_class_id(class_id);
    root_extraction_cache[root] = CachedRootExtraction{cost, choices};
}
