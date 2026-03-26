#include "pruner.h"

PruneSummary Pruner::run(
    const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings, size_t min_successful_samples) const {
    PruneSummary summary;

    std::unordered_map<Id, const ENode *> keep_choices;

    for (const auto &binding : bindings) {
        extractor.collect_selected_nodes_for_binding(roots, binding, keep_choices, summary.sampling_stats);
    }

    if (summary.sampling_stats.samples_succeeded < min_successful_samples) {
        return summary;
    }

    // Keep root classes only when they were not observed during sampling.
    // This remains lightweight while avoiding accidental empty-root pruning.
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

    summary.prune_result = egraph.prune_nodes_except(keep_choices);
    if (summary.prune_result.changed) {
        egraph.rebuild();
    }
    summary.ran = true;
    return summary;
}
