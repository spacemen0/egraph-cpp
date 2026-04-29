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

    compute_greedy_costs(size_bindings);

    Id max_id = 0;
    for (Id id : egraph.get_all_class_ids()) {
        max_id = std::max(max_id, id);
    }

    std::vector<Id> pending = {root};
    std::vector<size_t> pending_set(max_id + 1, 0);
    pending_set[root] = 1;
    std::vector<const ENode *> current_choices(max_id + 1, nullptr);
    std::vector<NumericSearchResult> best_results;
    double worst_selected_cost = std::numeric_limits<double>::infinity();

    std::vector<size_t> visited_buffer(max_id + 1, 0);
    std::vector<Id> stack_buffer;
    stack_buffer.reserve(max_id + 1);

    nodes_visited = 0;
    search_top_numeric_dags(
        root, pending, pending_set, current_choices, 0, size_bindings, 0.0, max_results, best_results,
        worst_selected_cost, visited_buffer, stack_buffer);

    if (enable_logging) {
        std::cout << "[Extractor] Visited " << nodes_visited << " nodes during numeric extraction." << std::endl;
    }
    if (best_results.empty()) {
        return {};
    }

    // Sort at the end to guarantee ascending order.
    std::sort(
        best_results.begin(), best_results.end(), [](const NumericSearchResult &lhs, const NumericSearchResult &rhs) {
        return lhs.cost < rhs.cost;
    });

    if (!size_bindings && max_results == 1) {
        cost_storage.store_root_extraction(root, best_results.front().cost, best_results.front().choices);
    }

    return best_results;
}

std::unordered_map<Id, const ENode *>
Extractor::convert_to_map(const std::vector<const ENode *> &choices, const std::vector<Id> &roots) const {
    std::unordered_map<Id, const ENode *> result;
    std::vector<Id> stack = roots;
    while (!stack.empty()) {
        Id current = stack.back();
        stack.pop_back();

        if (result.contains(current)) {
            continue;
        }

        const ENode *node = choices[current];
        if (node) {
            result[current] = node;
            for (Id child : node->get_children()) {
                stack.push_back(egraph.find_class_id(child));
            }
        }
    }
    return result;
}

void Extractor::compute_greedy_costs(const SizeBindings *size_bindings) const {
    greedy_costs.clear();
    greedy_choices.clear();

    auto all_class_ids = egraph.get_all_class_ids();
    for (Id id : all_class_ids) {
        greedy_costs[id] = std::numeric_limits<double>::infinity();
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (Id class_id : all_class_ids) {
            for (const ENode *node : egraph.get_class_nodes(class_id)) {
                Cost local_cost_v = node->compute_local_cost(egraph, size_bindings);
                if (!std::holds_alternative<double>(local_cost_v)) {
                    continue;
                }
                double current_node_cost = std::get<double>(local_cost_v);

                bool all_children_have_costs = true;
                for (Id child : node->get_children()) {
                    Id child_root = egraph.find_class_id(child);
                    if (greedy_costs[child_root] == std::numeric_limits<double>::infinity()) {
                        all_children_have_costs = false;
                        break;
                    }
                    current_node_cost += greedy_costs[child_root];
                }

                if (all_children_have_costs && current_node_cost < greedy_costs[class_id]) {
                    greedy_costs[class_id] = current_node_cost;
                    greedy_choices[class_id] = node;
                    changed = true;
                }
            }
        }
    }
}

ExtractionResult Extractor::greedy_extract(Id class_id, const SizeBindings &size_bindings) const {
    compute_greedy_costs(size_bindings.empty() ? nullptr : &size_bindings);
    Id root = egraph.find_class_id(class_id);
    if (greedy_costs.at(root) == std::numeric_limits<double>::infinity()) {
        throw std::runtime_error("Runtime error: no numeric DAG found for root class under supplied size bindings");
    }

    std::unordered_set<Id> visiting;
    return ExtractionResult{greedy_costs.at(root), build_expression(root, greedy_choices, visiting)};
}

std::vector<Extractor::SymbolicSearchResult> Extractor::find_symbolic_dags(Id root_class_id) const {
    Id root = egraph.find_class_id(root_class_id);

    Id max_id = 0;
    for (Id id : egraph.get_all_class_ids()) {
        max_id = std::max(max_id, id);
    }

    std::vector<Id> pending = {root};
    std::vector<size_t> pending_set(max_id + 1, 0);
    pending_set[root] = 1;
    std::vector<const ENode *> current_choices(max_id + 1, nullptr);
    std::vector<SymbolicSearchResult> results;
    SymbolicCost initial_cost;

    std::unordered_map<const ENode *, Cost> node_costs;
    for (Id class_id : egraph.get_all_class_ids()) {
        for (const ENode *node : egraph.get_class_nodes(class_id)) {
            node_costs[node] = node->compute_local_cost(egraph);
        }
    }

    std::vector<size_t> visited_buffer(max_id + 1, 0);
    std::vector<Id> stack_buffer;
    stack_buffer.reserve(max_id + 1);

    nodes_visited = 0;
    search_symbolic_dags(
        root, pending, pending_set, current_choices, 0, initial_cost, results, visited_buffer, stack_buffer,
        node_costs);

    if (enable_logging) {
        std::cout << "[Extractor] Visited " << nodes_visited << " nodes during symbolic extraction." << std::endl;
    }

    return results;
}

void Extractor::search_top_numeric_dags(
    Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set, std::vector<const ENode *> &current_choices,
    size_t chosen_count, const SizeBindings *size_bindings, double current_cost, size_t max_results,
    std::vector<NumericSearchResult> &best_results, double &worst_selected_cost, std::vector<size_t> &visited_buffer,
    std::vector<Id> &stack_buffer) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress (numeric): visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", chosen=" << chosen_count << ", best_results=" << best_results.size() << std::endl;
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
                best_results.push_back(NumericSearchResult{current_cost, convert_to_map(current_choices, {root})});

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

    if (chosen_count >= max_depth) {
        return;
    }

    Id current = pending.back();
    pending.pop_back();
    pending_set[current] = 0;

    struct Candidate {
        const ENode *node;
        double local_cost;
        double global_cost_estimate;
    };
    std::vector<Candidate> candidates;
    const auto &class_nodes = egraph.get_class_nodes(current);
    candidates.reserve(class_nodes.size());

    // Estimate global cost for each candidate using local cost + greedy costs of children
    for (const ENode *node : class_nodes) {
        Cost cost = node->compute_local_cost(egraph, size_bindings);
        if (std::holds_alternative<double>(cost)) {
            double local = std::get<double>(cost);
            double estimate = local;
            for (Id child : node->get_children()) {
                Id child_root = egraph.find_class_id(child);
                estimate += greedy_costs.at(child_root);
            }
            candidates.push_back({node, local, estimate});
        }
    }

    // Iterate candidates in order of estimated global cost (best first)
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.global_cost_estimate < b.global_cost_estimate;
    });

    for (const Candidate &candidate : candidates) {

        double next_cost = current_cost + candidate.local_cost;
        if (best_results.size() == max_results && next_cost >= worst_selected_cost) {
            continue;
        }

        if (creates_cycle(current, candidate.node, current_choices, visited_buffer, stack_buffer)) {
            continue;
        }

        current_choices[current] = candidate.node;

        int added_children_count = 0;
        for (Id child : candidate.node->get_children()) {
            Id child_root = egraph.find_class_id(child);
            if (current_choices[child_root] == nullptr) {
                // Ensure we don't add the same pending node twice from different paths
                if (!pending_set[child_root]) {
                    pending.push_back(child_root);
                    pending_set[child_root] = 1;
                    added_children_count++;
                }
            }
        }

        search_top_numeric_dags(
            root, pending, pending_set, current_choices, chosen_count + 1, size_bindings, next_cost, max_results,
            best_results, worst_selected_cost, visited_buffer, stack_buffer);

        for (int i = 0; i < added_children_count; ++i) {
            Id child_id = pending.back();
            pending.pop_back();
            pending_set[child_id] = 0;
        }
        current_choices[current] = nullptr;
    }

    pending.push_back(current);
    pending_set[current] = 1;
}

void Extractor::search_symbolic_dags(
    Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set, std::vector<const ENode *> &current_choices,
    size_t chosen_count, const SymbolicCost &current_cost, std::vector<SymbolicSearchResult> &results,
    std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer,
    const std::unordered_map<const ENode *, Cost> &node_costs) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress (symbolic): visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", chosen=" << chosen_count << ", results=" << results.size() << std::endl;
    }

    if (pending.empty()) {
        results.emplace_back(current_cost, convert_to_map(current_choices, {root}));
        return;
    }

    if (chosen_count >= max_depth) {
        return;
    }

    Id current = pending.back();
    pending.pop_back();
    pending_set[current] = 0;

    for (const ENode *candidate : egraph.get_class_nodes(current)) {
        const Cost &local_cost = node_costs.at(candidate);

        bool is_symbolic = std::holds_alternative<SymbolicCost>(local_cost);
        if (!is_symbolic && std::get<double>(local_cost) != 0.0) {
            continue;
        }

        if (!candidate->get_children().empty() &&
            creates_cycle(current, candidate, current_choices, visited_buffer, stack_buffer)) {
            continue;
        }

        current_choices[current] = candidate;

        int added_children_count = 0;
        for (Id child : candidate->get_children()) {
            Id child_root = egraph.find_class_id(child);
            if (current_choices[child_root] == nullptr) {
                if (!pending_set[child_root]) {
                    pending.push_back(child_root);
                    pending_set[child_root] = 1;
                    added_children_count++;
                }
            }
        }

        if (is_symbolic) {
            SymbolicCost next_cost = current_cost;
            const auto &sc = std::get<SymbolicCost>(local_cost);
            for (const auto &[m, c] : sc) {
                next_cost[m] += c;
            }
            search_symbolic_dags(
                root, pending, pending_set, current_choices, chosen_count + 1, next_cost, results, visited_buffer,
                stack_buffer, node_costs);
        } else {
            search_symbolic_dags(
                root, pending, pending_set, current_choices, chosen_count + 1, current_cost, results, visited_buffer,
                stack_buffer, node_costs);
        }

        // Backtrack
        for (int i = 0; i < added_children_count; ++i) {
            Id child_id = pending.back();
            pending.pop_back();
            pending_set[child_id] = 0;
        }
        current_choices[current] = nullptr;
    }

    // Restore pending state
    pending.push_back(current);
    pending_set[current] = 1;
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

ExtractionResult Extractor::extract(Id class_id, const SizeBindings &size_bindings) const {
    auto results = extract(class_id, 1, size_bindings);
    if (!results.empty()) {
        return results.front();
    }

    if (size_bindings.empty()) {
        throw std::runtime_error("Runtime error: no numeric DAG found for root class");
    } else {
        throw std::runtime_error(
            "Runtime error: no numeric DAG found for root class "
            "under supplied size bindings");
    }
}

std::vector<ExtractionResult>
Extractor::extract(Id class_id, size_t max_results, const SizeBindings &size_bindings) const {
    auto top_dags = find_top_numeric_dags(class_id, max_results, size_bindings.empty() ? nullptr : &size_bindings);
    std::vector<ExtractionResult> results;
    results.reserve(top_dags.size());
    for (const auto &dag : top_dags) {
        std::unordered_set<Id> visiting;
        results.emplace_back(dag.cost, build_expression(class_id, dag.choices, visiting));
    }
    return results;
}

bool Extractor::creates_cycle(
    Id current_class, const ENode *candidate_node, const std::vector<const ENode *> &current_choices,
    std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const {
    stack_buffer.clear();
    for (Id child : candidate_node->get_children()) {
        stack_buffer.push_back(egraph.find_class_id(child));
    }

    // Use a unique marker for each call to avoid clearing the whole buffer
    static thread_local size_t marker = 0;
    if (++marker == 0) {
        std::fill(visited_buffer.begin(), visited_buffer.end(), 0);
        marker = 1;
    }

    while (!stack_buffer.empty()) {
        Id node = stack_buffer.back();
        stack_buffer.pop_back();

        // If we loop back to the class we are currently trying to assign, it's a cycle.
        if (node == current_class) {
            return true;
        }

        // Only explore nodes we haven't checked yet
        if (visited_buffer[node] != marker) {
            visited_buffer[node] = marker;
            const ENode *chosen = current_choices[node];
            if (chosen) {
                // This child already has a chosen path, follow it downward
                for (Id next_child : chosen->get_children()) {
                    stack_buffer.push_back(egraph.find_class_id(next_child));
                }
            }
        }
    }

    return false;
}

std::vector<ExtractionResult> Extractor::extract_symbolic(Id class_id, bool build_expressions) const {
    auto symbolic_dags = find_symbolic_dags(class_id);
    std::vector<ExtractionResult> results;
    for (const auto &dag : symbolic_dags) {
        std::unordered_set<Id> visiting;
        if (build_expressions) {
            results.emplace_back(dag.cost, build_expression(class_id, dag.choices, visiting));
        } else {
            results.emplace_back(dag.cost, Expression());
        }
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