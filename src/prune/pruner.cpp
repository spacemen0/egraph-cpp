#include "pruner.h"
#include <iostream>

PruneResult Pruner::run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const {
    PruneResult result;

    std::unordered_map<Id, const ENode *> keep_choices;

    for (const auto &binding : bindings) {
        std::cout << "Pruning for one iteration..." << std::endl;
        extractor.collect_selected_nodes_for_binding(roots, binding, 10, keep_choices);
    };
    // Keep all root classes (if multiple roots were passed but only part of them were extractable)
    for (Id root : roots) {
        Id root_class = egraph.find_class_id(root);
        if (keep_choices.contains(root_class)) {
            continue;
        }

        const auto &class_nodes = egraph.get_class_nodes(root_class);
        if (!class_nodes.empty()) {
            keep_choices[root_class] = class_nodes.front();
        }
    }

    result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}
