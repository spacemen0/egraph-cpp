#pragma once

#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include <functional>
#include <utility>
#include <vector>

#include "egraph_config.h"

namespace egraph {
class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor, EGraphConfig config = EGraphConfig())
        : egraph(egraph), extractor(extractor), config(std::move(config)) {}

    PruneResult prune(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const;
    static PruneResult prune_symbolic_when_kernel_available(EGraph &egraph, const std::vector<Id> &roots = {});
    static PruneResult eliminate_unreachable_classes(EGraph &egraph, const std::vector<Id> &roots);

    void rewrite_and_prune(
        const std::vector<Id> &roots, Rewriter &rewriter, const std::vector<std::string> &size_keys,
        const std::function<void(int iteration)> &onIterationStart = nullptr,
        const std::function<void(int iteration, const PruneResult &)> &onIterationFinish = nullptr) const;

    void set_config(const EGraphConfig &cfg) { config = cfg; }
    const EGraphConfig &get_config() const { return config; }

  private:
    EGraph &egraph;
    Extractor &extractor;
    EGraphConfig config;
};

} // namespace egraph
