#include "cost_storage.h"
#include "e_graph.h"

CostStorage::CostStorage(const EGraph &egraph) : egraph(egraph)
{
}

void CostStorage::ensure_root_cost_cache_fresh()
{
    uint64_t revision = egraph.get_revision();
    if (root_cost_cache_revision != revision)
    {
        root_extraction_cache.clear();
        root_cost_cache_revision = revision;
    }
}

std::optional<CostStorage::CachedRootExtraction> CostStorage::cached_root_extraction(Id class_id)
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

void CostStorage::store_root_extraction(Id class_id, double cost, const std::unordered_map<Id, const ENode *> &choices)
{
    if (!egraph.is_clean() || !std::isfinite(cost) || choices.empty())
    {
        return;
    }
    ensure_root_cost_cache_fresh();

    Id root = egraph.find_class_id(class_id);
    root_extraction_cache[root] = CachedRootExtraction{cost, choices};
}
