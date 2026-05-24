#include "pruner.h"
#include "utils.h"

PruneResult
Pruner::run(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings, int max_results) const {
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

PruneResult Pruner::prune_symbolic_when_kernel_available() const {
    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (Id class_id : egraph.get_all_class_ids()) {
        const auto &nodes = egraph.get_class_nodes(class_id);
        bool has_kernel = false;
        for (const ENode *node : nodes) {
            auto atom = node->get_atom();
            if (std::holds_alternative<Op>(atom)) {
                auto op = std::get<Op>(atom);
                if (op == Op::Gemm || op == Op::Syrk || op == Op::Trsm || op == Op::Potrf || op == Op::Geqrf ||
                    op == Op::Gemv) {
                    has_kernel = true;
                    break;
                }
            }
        }

        if (has_kernel) {
            for (const ENode *node : nodes) {
                // Keep the node if it's NOT an Op, or if it IS a kernel op / Get
                auto atom = node->get_atom();
                if (std::holds_alternative<Op>(atom)) {
                    auto op = std::get<Op>(atom);
                    if (op == Op::Gemm || op == Op::Syrk || op == Op::Trsm || op == Op::Potrf || op == Op::Geqrf ||
                        op == Op::Gemv || op == Op::Get) {
                        keep_choices[class_id].insert(node);
                    }
                } else {
                    // String / Int atoms (variables/indices) are kept
                    keep_choices[class_id].insert(node);
                }
            }
        } else {
            // No kernel in this e-class, keep all nodes
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

void Pruner::rewrite_and_run(
    const std::vector<Id> &roots, Rewriter &rewriter, const PruneOptions &options,
    std::function<void(int iteration)> onIterationStart,
    std::function<void(int iteration, const PruneResult &)> onIterationFinish) const {
    for (int i = 0; i < options.num_iterations; ++i) {
        if (onIterationStart) {
            onIterationStart(i);
        }
        rewriter.reset();
        rewriter.apply_rewrites(options.rewrite_steps_per_iteration);

        const auto bindings = sample_size_bindings(options.prune_samples_per_iteration, 1, 1000, options.size_keys);
        const auto prune_result = run(roots, bindings, options.max_results_per_binding);

        if (onIterationFinish) {
            onIterationFinish(i, prune_result);
        }
    }
}
