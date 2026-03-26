#pragma once

#include "e_graph.h"
#include "extractor.h"

struct PruneSummary {
    PruneStats sampling_stats;
    PruneResult prune_result;
    bool ran = false;
};

class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor) : egraph(egraph), extractor(extractor) {}

    PruneSummary
    run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings,
        size_t min_successful_samples = 1) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};
