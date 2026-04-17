#pragma once

#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include <functional>
#include <string>
#include <vector>

struct PruneOptions {
    int outer_iterations = 8;
    int rewrite_steps_per_iteration = 10;
    size_t prune_samples_per_iteration = 50;
    std::vector<std::string> size_keys;
};

class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor) : egraph(egraph), extractor(extractor) {}

    PruneResult run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const;

    void rewrite_and_run(const std::vector<Id> &roots, Rewriter &rewriter, const PruneOptions &options,
                           std::function<void(int iteration, const PruneResult &)> callback = nullptr) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};
