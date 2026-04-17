#include "utils.h"
#include "pruner.h"

PruneResult Pruner::run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const {
    PruneResult result;

    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (const auto &binding : bindings) {
        extractor.collect_selected_nodes_for_binding(roots, binding, 3, keep_choices);
    };
    // Keep all root classes (if multiple roots were passed but only part of them were extractable)
    for (Id root : roots) {
        Id root_class = egraph.find_class_id(root);
        if (keep_choices.contains(root_class)) {
            continue;
        }

        const auto &class_nodes = egraph.get_class_nodes(root_class);
        if (!class_nodes.empty()) {
            keep_choices[root_class].insert(class_nodes.front());
        }
    }

    result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}


void Pruner::rewrite_and_run(const std::vector<Id> &roots, Rewriter &rewriter, const PruneOptions &options,
                               std::function<void(int iteration, const PruneResult &)> callback) const {
    for (int i = 0; i < options.outer_iterations; ++i) {
        rewriter.reset();
        rewriter.apply_rewrites(options.rewrite_steps_per_iteration);

        const auto bindings = sample_size_bindings(options.prune_samples_per_iteration, 1, 1000, options.size_keys);
        const auto prune_result = run(roots, bindings);

        if (callback) {
            callback(i, prune_result);
        }
    }
}
