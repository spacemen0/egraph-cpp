#include "pruner.h"
#include "rewrite_sets.h"
#include "utils.h"

namespace egraph {
PruneResult Pruner::prune(const std::vector<Id> &roots, const std::vector<SizeBindings> &bindings) const {
    PruneResult result;

    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (size_t i = 0; i < bindings.size(); ++i) {
        extractor.reset();
        bool use_dag = (i < bindings.size() * 0.2);
        extractor.collect_selected_nodes_for_binding(roots, bindings[i], keep_choices, use_dag);
    }
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

namespace {
std::unordered_set<Id> find_reachable_classes(
    const EGraph &egraph, const std::vector<Id> &roots,
    const std::unordered_map<Id, std::unordered_set<const ENode *>> *keep_choices = nullptr) {
    if (roots.empty()) {
        return {};
    }

    std::unordered_set<Id> reachable;
    std::vector<Id> stack;
    for (Id root : roots) {
        Id root_class = egraph.find_class_id(root);
        if (reachable.insert(root_class).second) {
            stack.push_back(root_class);
        }
    }

    while (!stack.empty()) {
        Id curr = stack.back();
        stack.pop_back();

        if (keep_choices) {
            auto it = keep_choices->find(curr);
            if (it == keep_choices->end()) {
                continue;
            }
            for (const ENode *node : it->second) {
                for (Id child : node->get_children()) {
                    Id child_class = egraph.find_class_id(child);
                    if (reachable.insert(child_class).second) {
                        stack.push_back(child_class);
                    }
                }
            }
        } else {
            for (const ENode *node : egraph.get_class_nodes(curr)) {
                for (Id child : node->get_children()) {
                    Id child_class = egraph.find_class_id(child);
                    if (reachable.insert(child_class).second) {
                        stack.push_back(child_class);
                    }
                }
            }
        }
    }
    return reachable;
}
} // namespace

PruneResult Pruner::eliminate_unreachable_classes(EGraph &egraph, const std::vector<Id> &roots) {
    if (roots.empty()) {
        return {};
    }

    auto reachable = find_reachable_classes(egraph, roots);
    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;
    for (Id class_id : reachable) {
        for (const ENode *node : egraph.get_class_nodes(class_id)) {
            keep_choices[class_id].insert(node);
        }
    }

    auto result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}

PruneResult Pruner::prune_symbolic_when_kernel_available(EGraph &egraph, const std::vector<Id> &roots) {
    std::unordered_map<Id, std::unordered_set<const ENode *>> keep_choices;

    for (Id class_id : egraph.get_all_class_ids()) {
        const auto &nodes = egraph.get_class_nodes(class_id);
        bool has_kernel = false;
        bool has_orgqr = false;
        for (const ENode *node : nodes) {
            auto atom = node->get_atom();
            if (std::holds_alternative<Op>(atom)) {
                auto op = std::get<Op>(atom);
                if (is_kernel_op(op)) {
                    has_kernel = true;
                    if (op == Op::Orgqr) {
                        has_orgqr = true;
                        break;
                    }
                }
            }
        }

        if (has_kernel) {
            for (const ENode *node : nodes) {
                auto atom = node->get_atom();
                if (std::holds_alternative<Op>(atom)) {
                    auto op = std::get<Op>(atom);
                    if (is_kernel_op(op)) {
                        keep_choices[class_id].insert(node);
                    } else if (op == Op::Get) {
                        Id index_id = node->get_children().at(1);
                        if (has_orgqr && get_int_from_eclass(egraph, index_id) == 0) {
                            continue; // Skip Get(Geqrf, 0) if Orgqr is available
                        }
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

    if (!roots.empty()) {
        auto reachable = find_reachable_classes(egraph, roots, &keep_choices);
        std::erase_if(keep_choices, [&](const auto &item) {
            return !reachable.contains(item.first);
        });
    }

    auto result = egraph.prune_nodes_except(keep_choices);
    if (result.changed) {
        egraph.rebuild();
    }
    return result;
}

void Pruner::rewrite_and_prune(
    const std::vector<Id> &roots, Rewriter &rewriter, const PrunerConfig &config, std::vector<std::string> size_keys,
    std::function<void(int iteration)> onIterationStart,
    std::function<void(int iteration, const PruneResult &)> onIterationFinish) const {
    for (int i = 0; i < config.num_iterations; ++i) {
        if (onIterationStart) {
            onIterationStart(i);
        }

        rewriter.reset();
        rewriter.apply_rewrites();

        auto lowering_config = rewriter.get_config();
        lowering_config.rewrite.enable_backoff = false;
        lowering_config.rewrite.enable_node_limit = false;
        Rewriter lowering_rewriter(egraph, build_rewrite_sets({"lowering"}), lowering_config);
        lowering_rewriter.apply_rewrites();

        prune_symbolic_when_kernel_available(egraph, roots);
        const auto bindings = sample_size_bindings(
            config.prune_samples_per_iteration, 10, 5000, size_keys, static_cast<unsigned int>(99 + i),
            &egraph.get_property_table());
        const auto prune_result = prune(roots, bindings);

        // Eliminate unreachable orphan classes created by pruning
        eliminate_unreachable_classes(egraph, roots);

        if (onIterationFinish) {
            onIterationFinish(i, prune_result);
        }
    }
}

} // namespace egraph
