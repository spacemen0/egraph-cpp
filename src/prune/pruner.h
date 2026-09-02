#pragma once

#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include <functional>
#include <vector>

#include "egraph_config.h"

namespace egraph {
class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor) : egraph(egraph), extractor(extractor) {}

    PruneResult prune(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const;
    static PruneResult prune_symbolic_when_kernel_available(EGraph &egraph, const std::vector<Id> &roots = {});
    static PruneResult eliminate_unreachable_classes(EGraph &egraph, const std::vector<Id> &roots);

    void rewrite_and_prune(
        const std::vector<Id> &roots, Rewriter &rewriter, const PrunerConfig &config,
        std::vector<std::string> size_keys, std::function<void(int iteration)> onIterationStart = nullptr,
        std::function<void(int iteration, const PruneResult &)> onIterationFinish = nullptr) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};

} // namespace egraph
