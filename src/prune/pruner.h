#pragma once

#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include <functional>
#include <string>
#include <vector>

struct PruneOptions {
    int num_iterations;
    int rewrite_steps_per_iteration;
    int prune_samples_per_iteration;
    int max_results_per_binding;
    std::vector<std::string> size_keys;
};

class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor) : egraph(egraph), extractor(extractor) {}

    PruneResult run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings, int max_results) const;

    void rewrite_and_run(
        const std::vector<Id> &roots, Rewriter &rewriter, const PruneOptions &options,
        std::function<void(int iteration, const PruneResult &)> callback = nullptr) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};
