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

PruneResult Pruner::prune_symbolic_dominance(EGraph &egraph) {
    std::unordered_map<Id, SymbolicCost> class_min_cost;
    for (Id id : egraph.get_all_class_ids()) {
        const auto &nodes = egraph.get_class_nodes(id);
        if (!nodes.empty()) {
            Cost c = nodes.front()->compute_local_cost(egraph);
            if (std::holds_alternative<SymbolicCost>(c))
                class_min_cost[id] = std::get<SymbolicCost>(c);
            else
                class_min_cost[id] = SymbolicCost{};
        }
    }

    bool changed = true;
    for (int iter = 0; iter < 10 && changed; ++iter) {
        changed = false;
        for (Id class_id : egraph.get_all_class_ids()) {
            SymbolicCost best_cost = class_min_cost[class_id];
            bool first = true;
            for (const ENode *node : egraph.get_class_nodes(class_id)) {
                Cost local_c = node->compute_local_cost(egraph);
                SymbolicCost c =
                    std::holds_alternative<SymbolicCost>(local_c) ? std::get<SymbolicCost>(local_c) : SymbolicCost{};
                for (Id child : node->get_children()) {
                    c = c + class_min_cost[egraph.find_class_id(child)];
                }
                if (first || strictly_dominates(best_cost, c)) { // best_cost is worse than c
                    best_cost = c;
                    first = false;
                }
            }
            if (strictly_dominates(class_min_cost[class_id], best_cost)) {
                class_min_cost[class_id] = best_cost;
                changed = true;
            }
        }
    }

    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;
    for (Id class_id : egraph.get_all_class_ids()) {
        const auto &nodes = egraph.get_class_nodes(class_id);
        std::vector<std::pair<const ENode *, SymbolicCost>> node_costs;
        for (const ENode *node : nodes) {
            Cost local_c = node->compute_local_cost(egraph);
            SymbolicCost c =
                std::holds_alternative<SymbolicCost>(local_c) ? std::get<SymbolicCost>(local_c) : SymbolicCost{};
            for (Id child : node->get_children()) {
                c = c + class_min_cost[egraph.find_class_id(child)];
            }
            node_costs.push_back({node, c});
        }

        for (size_t i = 0; i < node_costs.size(); ++i) {
            bool dominated = false;
            for (size_t j = 0; j < node_costs.size(); ++j) {
                if (i == j)
                    continue;

                // Only compare nodes with the exact same children
                const auto &children_a = node_costs[i].first->get_children();
                const auto &children_b = node_costs[j].first->get_children();

                bool same_children = true;
                if (children_a.size() != children_b.size()) {
                    same_children = false;
                } else {
                    for (size_t k = 0; k < children_a.size(); ++k) {
                        if (egraph.find_class_id(children_a[k]) != egraph.find_class_id(children_b[k])) {
                            same_children = false;
                            break;
                        }
                    }
                }

                if (same_children && strictly_dominates(node_costs[i].second, node_costs[j].second)) {
                    dominated = true;
                    break;
                }
            }
            if (!dominated) {
                keep_choices[class_id].insert(node_costs[i].first);
            } else {
                std::cout << "Pruned " << node_costs[i].first->format() << std::endl;
            }
        }

        // Safety check: never completely empty a class
        if (keep_choices[class_id].empty() && !nodes.empty()) {
            keep_choices[class_id].insert(nodes.front());
        }
    }

    auto result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}

void Pruner::rewrite_and_prune(
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
        const auto prune_result = prune(roots, bindings, options.max_results_per_binding);

        if (onIterationFinish) {
            onIterationFinish(i, prune_result);
        }
    }
}
