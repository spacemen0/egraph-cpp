#pragma once

#include "e_node.h"
#include "types.h"
#include <cstdint>
#include <optional>
#include <unordered_map>

class EGraph;

class CostStorage
{
public:
    struct CachedRootExtraction
    {
        double cost;
        std::unordered_map<Id, const ENode *> choices;
    };

    void compute();
    Cost eclass_cost(Id class_id) const;
    Cost node_cost(const ENode *node);
    const ENode *best_node(Id class_id) const;
    std::optional<CachedRootExtraction> cached_root_extraction(Id class_id) const;
    void store_root_extraction(Id class_id, double cost, const std::unordered_map<Id, const ENode *> &choices) const;

private:
    explicit CostStorage(const EGraph &egraph);
    const EGraph &egraph;
    std::unordered_map<Id, Cost> e_class_costs;
    std::unordered_map<Id, const ENode *> best_nodes_in_e_class;
    std::unordered_map<const ENode *, Cost, ENodePtrHash, ENodePtrEqual> node_costs;
    mutable std::unordered_map<Id, CachedRootExtraction> root_extraction_cache;
    mutable uint64_t root_cost_cache_revision = 0;

    void ensure_root_cost_cache_fresh() const;

    friend class EGraph;
};
