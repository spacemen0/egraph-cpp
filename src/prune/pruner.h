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

    PruneResult prune(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings, int max_results) const;
    static PruneResult prune_symbolic_when_kernel_available(EGraph &egraph);
    static PruneResult prune_symbolic_dominance(EGraph &egraph);

    void rewrite_and_prune(
        const std::vector<Id> &roots, Rewriter &rewriter, const PruneOptions &options,
        std::function<void(int iteration)> onIterationStart = nullptr,
        std::function<void(int iteration, const PruneResult &)> onIterationFinish = nullptr) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};
