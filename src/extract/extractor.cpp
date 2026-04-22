#include "extractor.h"
#include "basic_types.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace {
constexpr size_t kExtractorProgressLogEvery = 1000000;
}

Extractor::Extractor(EGraph &egraph, CostStorage &cost_storage, bool enable_logging)
    : egraph(egraph), cost_storage(cost_storage), enable_logging(enable_logging) {}

std::vector<Extractor::NumericSearchResult>
Extractor::find_top_numeric_dags(Id root_class_id, size_t max_results, const SizeBindings *size_bindings) const {
    if (max_results == 0) {
        return {};
    }

    Id root = egraph.find_class_id(root_class_id);
    if (!size_bindings && max_results == 1) {
        if (auto cached = cost_storage.cached_root_extraction(root); cached.has_value()) {
            return {NumericSearchResult{cached->cost, cached->choices}};
        }
    }

    // Pass the pending queue as a simple stack of IDs
    std::vector<Id> pending = {root};
    std::unordered_set<Id> pending_set = {root};
    std::unordered_map<Id, const ENode *> current_choices;
    std::vector<NumericSearchResult> best_results;
    double worst_selected_cost = std::numeric_limits<double>::infinity();

    nodes_visited = 0;
    search_top_numeric_dags(
        pending, pending_set, current_choices, size_bindings, 0.0, max_results, best_results, worst_selected_cost);

    if (enable_logging) {
        std::cout << "[Extractor] Visited " << nodes_visited << " nodes during numeric extraction." << std::endl;
    }
    if (best_results.empty()) {
        return {};
    }

    // The heap guarantees the largest elements are at the front. We sort at the end to guarantee ascending order.
    std::sort(
        best_results.begin(), best_results.end(), [](const NumericSearchResult &lhs, const NumericSearchResult &rhs) {
        return lhs.cost < rhs.cost;
    });

    if (!size_bindings && max_results == 1) {
        cost_storage.store_root_extraction(root, best_results.front().cost, best_results.front().choices);
    }

    return best_results;
}

std::vector<Extractor::SymbolicSearchResult> Extractor::find_symbolic_dags(Id root_class_id) const {
    Id root = egraph.find_class_id(root_class_id);

    std::vector<Id> pending = {root};
    std::unordered_set<Id> pending_set = {root};
    std::unordered_map<Id, const ENode *> current_choices;
    std::vector<SymbolicSearchResult> results;
    SymbolicCost initial_cost;

    nodes_visited = 0;
    search_symbolic_dags(pending, pending_set, current_choices, initial_cost, results);

    if (enable_logging) {
        std::cout << "[Extractor] Visited " << nodes_visited << " nodes during symbolic extraction." << std::endl;
    }

    return results;
}

void Extractor::search_top_numeric_dags(
    std::vector<Id> &pending, std::unordered_set<Id> &pending_set,
    std::unordered_map<Id, const ENode *> &current_choices, const SizeBindings *size_bindings, double current_cost,
    size_t max_results, std::vector<NumericSearchResult> &best_results, double &worst_selected_cost) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress (numeric): visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", chosen=" << current_choices.size() << ", best_results=" << best_results.size() << std::endl;
    }

    if (pending.empty()) {
        if (best_results.size() < max_results || current_cost < worst_selected_cost) {
            bool has_unique_cost = true;
            for (const auto &res : best_results) {
                if (std::abs(res.cost - current_cost) < 1e-9) {
                    has_unique_cost = false;
                    break;
                }
            }
            if (has_unique_cost) {
                best_results.push_back(NumericSearchResult{current_cost, current_choices});

                // Push into Max-Heap based on cost (worst cost bubbles to the front)
                std::push_heap(
                    best_results.begin(), best_results.end(),
                    [](const NumericSearchResult &a, const NumericSearchResult &b) {
                    return a.cost < b.cost;
                });

                // Evict worst result if we exceed max_results
                if (best_results.size() > max_results) {
                    std::pop_heap(
                        best_results.begin(), best_results.end(),
                        [](const NumericSearchResult &a, const NumericSearchResult &b) {
                        return a.cost < b.cost;
                    });
                    best_results.pop_back();
                }

                // Update worst_selected_cost boundary
                if (best_results.size() == max_results) {
                    worst_selected_cost = best_results.front().cost;
                } else {
                    worst_selected_cost = std::numeric_limits<double>::infinity();
                }
            }
        }
        return;
    }

    Id current = pending.back();
    pending.pop_back();
    pending_set.erase(current);

    for (const ENode *candidate : egraph.get_class_nodes(current)) {
        Cost local_cost = candidate->compute_local_cost(egraph, size_bindings);

        if (!std::holds_alternative<double>(local_cost)) {
            continue;
        }

        double next_cost = current_cost + std::get<double>(local_cost);
        if (best_results.size() == max_results && next_cost >= worst_selected_cost) {
            continue;
        }

        if (creates_cycle(current, candidate, current_choices)) {
            continue;
        }

        current_choices[current] = candidate;

        int added_children_count = 0;
        for (Id child : candidate->get_children()) {
            Id child_root = egraph.find_class_id(child);
            if (!current_choices.contains(child_root)) {
                // Ensure we don't add the same pending node twice from different paths
                if (!pending_set.contains(child_root)) {
                    pending.push_back(child_root);
                    pending_set.insert(child_root);
                    added_children_count++;
                }
            }
        }

        search_top_numeric_dags(
            pending, pending_set, current_choices, size_bindings, next_cost, max_results, best_results,
            worst_selected_cost);

        for (int i = 0; i < added_children_count; ++i) {
            Id child_id = pending.back();
            pending.pop_back();
            pending_set.erase(child_id);
        }
        current_choices.erase(current);
    }

    pending.push_back(current);
    pending_set.insert(current);
}

void Extractor::search_symbolic_dags(
    std::vector<Id> &pending, std::unordered_set<Id> &pending_set,
    std::unordered_map<Id, const ENode *> &current_choices, SymbolicCost current_cost,
    std::vector<SymbolicSearchResult> &results) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress (symbolic): visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", chosen=" << current_choices.size() << ", results=" << results.size() << std::endl;
    }

    if (pending.empty()) {
        results.emplace_back(current_cost, current_choices);
        return;
    }

    Id current = pending.back();
    pending.pop_back();
    pending_set.erase(current);

    for (const ENode *candidate : egraph.get_class_nodes(current)) {
        Cost local_cost = candidate->compute_local_cost(egraph);

        SymbolicCost next_cost = current_cost;
        if (std::holds_alternative<SymbolicCost>(local_cost)) {
            next_cost = next_cost + std::get<SymbolicCost>(local_cost);
        } else if (std::holds_alternative<double>(local_cost) && std::get<double>(local_cost) != 0.0) {
            continue;
        }

        if (creates_cycle(current, candidate, current_choices)) {
            continue;
        }

        current_choices[current] = candidate;

        int added_children_count = 0;
        for (Id child : candidate->get_children()) {
            Id child_root = egraph.find_class_id(child);
            if (!current_choices.contains(child_root)) {
                if (!pending_set.contains(child_root)) {
                    pending.push_back(child_root);
                    pending_set.insert(child_root);
                    added_children_count++;
                }
            }
        }

        search_symbolic_dags(pending, pending_set, current_choices, next_cost, results);

        // Backtrack
        for (int i = 0; i < added_children_count; ++i) {
            Id child_id = pending.back();
            pending.pop_back();
            pending_set.erase(child_id);
        }
        current_choices.erase(current);
    }

    // Restore pending state
    pending.push_back(current);
    pending_set.insert(current);
}

Expression Extractor::build_expression(
    Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const {
    Id root = egraph.find_class_id(class_id);
    auto it = choices.find(root);
    if (it == choices.end()) {
        throw std::runtime_error("Runtime error: missing choice for reachable e-class");
    }

    if (visiting.contains(root)) {
        throw std::runtime_error("Runtime error: cycle detected while building expression");
    }
    visiting.insert(root);

    std::vector<Expression> children;
    children.reserve(it->second->get_children().size());
    for (Id child_id : it->second->get_children()) {
        children.push_back(build_expression(child_id, choices, visiting));
    }
    visiting.erase(root);
    return Expression(it->second->get_atom(), children);
}

ExtractionResult Extractor::extract(Id class_id) const {
    auto results = extract(class_id, 1);
    if (!results.empty()) {
        return results.front();
    }

    throw std::runtime_error("Runtime error: no numeric DAG found for root class");
}

std::vector<ExtractionResult> Extractor::extract(Id class_id, size_t max_results) const {
    auto top_dags = find_top_numeric_dags(class_id, max_results);
    std::vector<ExtractionResult> results;
    results.reserve(top_dags.size());
    for (const auto &dag : top_dags) {
        std::unordered_set<Id> visiting;
        results.emplace_back(dag.cost, build_expression(class_id, dag.choices, visiting));
    }
    return results;
}

ExtractionResult Extractor::extract(Id class_id, const SizeBindings &size_bindings) const {
    auto results = extract(class_id, size_bindings, 1);
    if (!results.empty()) {
        return results.front();
    }

    throw std::runtime_error(
        "Runtime error: no numeric DAG found for root class "
        "under supplied size bindings");
}

std::vector<ExtractionResult>
Extractor::extract(Id class_id, const SizeBindings &size_bindings, size_t max_results) const {
    auto top_dags = find_top_numeric_dags(class_id, max_results, &size_bindings);
    std::vector<ExtractionResult> results;
    results.reserve(top_dags.size());
    for (const auto &dag : top_dags) {
        std::unordered_set<Id> visiting;
        results.emplace_back(dag.cost, build_expression(class_id, dag.choices, visiting));
    }
    return results;
}

bool Extractor::creates_cycle(
    Id current_class, const ENode *candidate, const std::unordered_map<Id, const ENode *> &current_choices) const {
    std::vector<Id> stack;
    for (Id child : candidate->get_children()) {
        stack.push_back(egraph.find_class_id(child));
    }

    std::unordered_set<Id> visited;

    while (!stack.empty()) {
        Id node = stack.back();
        stack.pop_back();

        // If we loop back to the class we are currently trying to assign, it's a cycle.
        if (node == current_class) {
            return true;
        }

        // Only explore nodes we haven't checked yet
        if (visited.insert(node).second) {
            auto it = current_choices.find(node);
            if (it != current_choices.end()) {
                // This child already has a chosen path, follow it downward
                for (Id next_child : it->second->get_children()) {
                    stack.push_back(egraph.find_class_id(next_child));
                }
            }
        }
    }

    return false;
}

std::vector<ExtractionResult> Extractor::extract_symbolic(Id class_id) const {
    auto symbolic_dags = find_symbolic_dags(class_id);
    std::vector<ExtractionResult> results;
    for (const auto &dag : symbolic_dags) {
        std::unordered_set<Id> visiting;
        results.emplace_back(dag.cost, build_expression(class_id, dag.choices, visiting));
    }
    return results;
}

bool Extractor::collect_selected_nodes_for_binding(
    const std::vector<Id> &roots, const SizeBindings &size_bindings, size_t max_results,
    std::unordered_map<Id, std::unordered_set<const ENode *>> &selected_choices) const {
    bool any_root_succeeded = false;

    for (Id root : roots) {
        auto results = find_top_numeric_dags(root, max_results, &size_bindings);
        for (const auto &result : results) {
            any_root_succeeded = true;
            for (const auto &[class_id, node] : result.choices) {
                selected_choices[class_id].insert(node);
            }
        }
    }

    return any_root_succeeded;
}