#pragma once

#include "e_graph.h"
#include "extractor.h"

class Pruner {
  public:
    Pruner(EGraph &egraph, Extractor &extractor) : egraph(egraph), extractor(extractor) {}

    PruneResult run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const;

  private:
    EGraph &egraph;
    Extractor &extractor;
};
