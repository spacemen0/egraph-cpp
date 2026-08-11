#include "pruner.h"
#include "utils.h"

PruneResult
Pruner::prune(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings, int max_results) const {
    PruneResult result;

    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (const auto &binding : bindings) {
        extractor.reset();
        extractor.collect_selected_nodes_for_binding(roots, binding, max_results, keep_choices);
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

PruneResult Pruner::prune_symbolic_when_kernel_available(EGraph &egraph) {
    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (Id class_id : egraph.get_all_class_ids()) {
        const auto &nodes = egraph.get_class_nodes(class_id);
        bool has_kernel = false;
        for (const ENode *node : nodes) {
            auto atom = node->get_atom();
            if (std::holds_alternative<Op>(atom)) {
                auto op = std::get<Op>(atom);
                if (is_kernel_op(op)) {
                    has_kernel = true;
                    break;
                }
            }
        }

        if (has_kernel) {
            for (const ENode *node : nodes) {
                auto atom = node->get_atom();
                if (std::holds_alternative<Op>(atom)) {
                    auto op = std::get<Op>(atom);
                    if (is_kernel_op(op) || op == Op::Get || op == Op::Tr || op == Op::Scale || op == Op::Add ||
                        op == Op::Minus) {
                        keep_choices[class_id].insert(node);
                    }
                } else {
                    // Constants
                    keep_choices[class_id].insert(node);
                }
            }
        } else {
            for (const ENode *node : nodes) {
                keep_choices[class_id].insert(node);
            }
        }
    }

    auto result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}

void Pruner::rewrite_and_prune(
    const std::vector<Id> &roots, Rewriter &rewriter, const PrunerConfig &config,
    std::function<void(int iteration)> onIterationStart,
    std::function<void(int iteration, const PruneResult &)> onIterationFinish) const {
    for (int i = 0; i < config.num_iterations; ++i) {
        if (onIterationStart) {
            onIterationStart(i);
        }
        rewriter.reset();
        rewriter.apply_rewrites(config.rewrite_steps_per_iteration);

        const auto bindings =
            sample_size_bindings(config.prune_samples_per_iteration, 1, 1000, config.size_keys, config.seed + i);
        const auto prune_result = prune(roots, bindings, config.max_results_per_binding);

        if (onIterationFinish) {
            onIterationFinish(i, prune_result);
        }
    }
}
