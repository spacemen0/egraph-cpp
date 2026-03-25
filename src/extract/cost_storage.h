#pragma once

#include "e_node.h"
#include <cstdint>
#include <optional>
#include <unordered_map>

class EGraph;

class CostStorage {
  public:
    struct CachedRootExtraction {
        double cost;
        std::unordered_map<Id, const ENode *> choices;
    };
    CostStorage(const EGraph &egraph);
    std::optional<CachedRootExtraction> cached_root_extraction(Id class_id);
    void store_root_extraction(Id class_id, double cost, const std::unordered_map<Id, const ENode *> &choices);

  private:
    const EGraph &egraph;
    std::unordered_map<Id, CachedRootExtraction> root_extraction_cache;
    uint64_t root_cost_cache_revision = 0;

    void ensure_root_cost_cache_fresh();
};
