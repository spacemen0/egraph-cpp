#include "extractor.h"
#include "basic_types.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace {
constexpr size_t kExtractorProgressLogEvery = 1000000;
}

Extractor::Extractor(
    EGraph &egraph, CostStorage &cost_storage, bool enable_logging, size_t max_depth, size_t node_visit_limit)
    : egraph(egraph), cost_storage(cost_storage), enable_logging(enable_logging), max_depth(max_depth),
      node_visit_limit(node_visit_limit) {}

void Extractor::reset() const {
    tree_cost.clear();
    minimal_possible_costs.clear();
    minimal_possible_sizes.clear();
    greedy_choices.clear();
    nodes_visited = 0;
}
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

    initial_tree_search_pass(size_bindings);

    if (tree_cost[root] == std::numeric_limits<double>::infinity()) {
        return {};
    }

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

    // A single stack buffer for cycle detection to avoid reallocations during deep recursion
    std::vector<Id> stack_buffer;
    stack_buffer.reserve(max_id + 1);

    nodes_visited = 0;

    // Heuristic
    if (tree_cost[root] != std::numeric_limits<double>::infinity()) {
        std::vector<const ENode *> greedy_vec(max_id + 1, nullptr);
        for (const auto &[id, node] : greedy_choices) {
            greedy_vec[id] = node;
        }
        best_results.push_back(NumericSearchResult{tree_cost[root], convert_to_map(greedy_vec, {root})});

        if (best_results.size() == max_results) {
            worst_selected_cost = tree_cost[root];
        } else {
            // Heuristic
            worst_selected_cost = tree_cost[root] * 10.0;
        }
    }

    if (!should_prune_numeric_search(
            0, 0.0, minimal_possible_costs[root], minimal_possible_sizes[root], max_results, best_results.size(),
            worst_selected_cost)) {
        search_top_numeric_dags(
            root, pending, pending_set, current_choices, 0, size_bindings, 0.0, minimal_possible_costs[root],
            minimal_possible_sizes[root], max_results, best_results, worst_selected_cost, visited_buffer, stack_buffer);
    }

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

void Extractor::initial_tree_search_pass(const SizeBindings *size_bindings) const {
    tree_cost.clear();
    minimal_possible_costs.clear();
    minimal_possible_sizes.clear();
    greedy_choices.clear();

    auto all_class_ids = egraph.get_all_class_ids();
    for (Id id : all_class_ids) {
        tree_cost[id] = std::numeric_limits<double>::infinity();
        minimal_possible_costs[id] = std::numeric_limits<double>::infinity();
        minimal_possible_sizes[id] = std::numeric_limits<double>::infinity();
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (Id class_id : all_class_ids) {
            for (const ENode *node : egraph.get_class_nodes(class_id)) {
                Cost local_cost = node->compute_local_cost(egraph, size_bindings);
                if (!std::holds_alternative<double>(local_cost)) {
                    continue;
                }
                double local = std::get<double>(local_cost);

                // Lower bounds for dag cost and size
                double max_child_cost = 0;
                size_t max_child_size = 0;
                bool children_incomplete_for_lb = false;

                // Tree cost
                double current_node_greedy_cost = local;
                bool children_incomplete_for_greedy = false;

                for (Id child : node->get_children()) {
                    Id child_root = egraph.find_class_id(child);

                    if (minimal_possible_costs[child_root] == std::numeric_limits<double>::infinity()) {
                        children_incomplete_for_lb = true;
                    } else {

                        // use std::max because this is at least the cost of all children, even all other children are
                        // free by sharing.
                        max_child_cost = std::max(max_child_cost, minimal_possible_costs[child_root]);
                        max_child_size = std::max(max_child_size, minimal_possible_sizes[child_root]);
                    }

                    if (tree_cost[child_root] == std::numeric_limits<double>::infinity()) {
                        children_incomplete_for_greedy = true;
                    } else {
                        current_node_greedy_cost += tree_cost[child_root];
                    }
                }

                if (!children_incomplete_for_lb) {
                    double node_lb_cost = local + max_child_cost;
                    size_t node_lb_size = 1 + max_child_size;
                    if (node_lb_cost < minimal_possible_costs[class_id]) {
                        minimal_possible_costs[class_id] = node_lb_cost;
                        minimal_possible_sizes[class_id] = node_lb_size;
                        changed = true;
                    }
                }

                if (!children_incomplete_for_greedy) {
                    if (current_node_greedy_cost < tree_cost[class_id]) {
                        tree_cost[class_id] = current_node_greedy_cost;
                        greedy_choices[class_id] = node;
                        changed = true;
                    }
                }
            }
        }
    }
}

ExtractionResult Extractor::tree_extract(Id class_id, const SizeBindings &size_bindings) const {
    initial_tree_search_pass(size_bindings.empty() ? nullptr : &size_bindings);
    Id root = egraph.find_class_id(class_id);
    if (tree_cost.at(root) == std::numeric_limits<double>::infinity()) {
        throw std::runtime_error("Runtime error: no numeric DAG found for root class under supplied size bindings");
    }

    std::unordered_set<Id> visiting;
    return ExtractionResult{tree_cost.at(root), build_expression(root, greedy_choices, visiting)};
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

void Extractor::record_numeric_result(
    Id root, const std::vector<const ENode *> &current_choices, double current_cost, size_t max_results,
    std::vector<NumericSearchResult> &best_results, double &worst_selected_cost) const {

    // Check if the current solution has a unique cost to avoid storing redundant results
    bool has_unique_cost = true;
    for (const auto &res : best_results) {
        if (std::abs(res.cost - current_cost) < 1e-9) {
            has_unique_cost = false;
            break;
        }
    }

    if (has_unique_cost) {
        best_results.push_back(NumericSearchResult{current_cost, convert_to_map(current_choices, {root})});

        std::push_heap(
            best_results.begin(), best_results.end(), [](const NumericSearchResult &a, const NumericSearchResult &b) {
            return a.cost < b.cost;
        });

        if (best_results.size() > max_results) {
            std::pop_heap(
                best_results.begin(), best_results.end(),
                [](const NumericSearchResult &a, const NumericSearchResult &b) {
                return a.cost < b.cost;
            });
            best_results.pop_back();
        }

        if (best_results.size() == max_results) {
            worst_selected_cost = best_results.front().cost;
        }
    }
}

bool Extractor::should_prune_numeric_search(
    size_t chosen_count, double current_cost, double pending_lb_cost, size_t pending_lb_size, size_t max_results,
    size_t best_results_count, double worst_selected_cost) const {

    if (std::max(chosen_count, pending_lb_size) > max_depth) {
        return true;
    }

    if (best_results_count == max_results && std::max(current_cost, pending_lb_cost) >= worst_selected_cost) {
        return true;
    }

    return false;
}

std::vector<Extractor::Candidate>
Extractor::evaluate_node_candidates(Id eclass_id, const SizeBindings *size_bindings) const {

    std::vector<Candidate> candidates;
    const auto &class_nodes = egraph.get_class_nodes(eclass_id);
    candidates.reserve(class_nodes.size());

    for (const ENode *node : class_nodes) {

        Cost cost = node->compute_local_cost(egraph, size_bindings);
        if (std::holds_alternative<double>(cost)) {
            double local = std::get<double>(cost);
            double tree_est = local;
            double max_child_cost = 0;
            size_t max_child_size = 0;
            bool any_child_infinite = false;

            for (Id child : node->get_children()) {
                Id child_root = egraph.find_class_id(child);
                double child_greedy = tree_cost.at(child_root);

                if (child_greedy == std::numeric_limits<double>::infinity()) {
                    any_child_infinite = true;
                    break;
                }

                tree_est += child_greedy;
                max_child_cost = std::max(max_child_cost, minimal_possible_costs.at(child_root));
                max_child_size = std::max(max_child_size, minimal_possible_sizes.at(child_root));
            }

            if (!any_child_infinite) {
                candidates.push_back({node, local, tree_est, local + max_child_cost, 1 + max_child_size});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.tree_cost_heuristic < b.tree_cost_heuristic;
    });

    return candidates;
}

void Extractor::search_top_numeric_dags(
    Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set, std::vector<const ENode *> &current_choices,
    size_t chosen_count, const SizeBindings *size_bindings, double current_cost, double current_pending_lb_cost,
    double current_pending_lb_size, size_t max_results, std::vector<NumericSearchResult> &best_results,
    double &worst_selected_cost, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress (numeric): visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", chosen=" << chosen_count << ", best_results=" << best_results.size() << std::endl;
    }

    if (pending.empty()) {
        record_numeric_result(root, current_choices, current_cost, max_results, best_results, worst_selected_cost);
        return;
    }

    if (nodes_visited >= node_visit_limit) {
        if (enable_logging) {
            std::cout << "[Extractor] Node visit limit reached, stopping search." << std::endl;
        }
        return;
    }

    // Heuristic: Select the pending e-class with the highest lower bound on cost, because we want to make decisions on
    // the most expensive parts of the DAG first.
    auto it = std::max_element(pending.begin(), pending.end(), [&](Id a, Id b) {
        return minimal_possible_costs.at(a) < minimal_possible_costs.at(b);
    });

    Id current = *it;
    size_t original_index = std::distance(pending.begin(), it);
    size_t last_index = pending.size() - 1;

    std::swap(pending[original_index], pending[last_index]);
    pending.pop_back();
    pending_set[current] = 0;

    // Pre-calculate lower bounds for the remaining e-classes to pass down to children
    double remaining_work_cost_lower_bound = 0;
    size_t remaining_work_size_lower_bound = 0;
    for (Id p : pending) {
        remaining_work_cost_lower_bound = std::max(remaining_work_cost_lower_bound, minimal_possible_costs.at(p));
        remaining_work_size_lower_bound = std::max(remaining_work_size_lower_bound, minimal_possible_sizes.at(p));
    }

    std::vector<Candidate> candidates = evaluate_node_candidates(current, size_bindings);

    for (const Candidate &candidate : candidates) {
        double next_cost = current_cost + candidate.local_cost;

        // Calculate combined lower bounds for the entire DAG (use max instead of sum because lower bounds are not
        // additive)
        double combined_lb_cost = std::max(remaining_work_cost_lower_bound, candidate.minimal_possible_cost);
        size_t combined_lb_size = std::max(remaining_work_size_lower_bound, candidate.minimal_possible_size);

        if (creates_cycle(current, candidate.node, current_choices, visited_buffer, stack_buffer)) {
            continue;
        }
        if (best_results.size() == max_results && std::max(current_cost, combined_lb_cost) >= worst_selected_cost) {
            continue;
        }
        // Early depth pruning
        if (std::max(chosen_count, combined_lb_size) > max_depth) {
            continue;
        }

        current_choices[current] = candidate.node;

        // Add children to pending list if they haven't been chosen yet
        int added_children_count = 0;
        double next_step_lb_cost = remaining_work_cost_lower_bound;
        size_t next_step_lb_size = remaining_work_size_lower_bound;
        for (Id child : candidate.node->get_children()) {
            Id child_root = egraph.find_class_id(child);

            // This accounts for Common Subexpression Elimination
            if (current_choices[child_root] == nullptr && !pending_set[child_root]) {
                pending.push_back(child_root);
                pending_set[child_root] = 1;
                added_children_count++;
                next_step_lb_cost = std::max(next_step_lb_cost, minimal_possible_costs.at(child_root));
                next_step_lb_size = std::max(next_step_lb_size, minimal_possible_sizes.at(child_root));
            }
        }

        if (!should_prune_numeric_search(
                chosen_count + 1, next_cost, next_step_lb_cost, next_step_lb_size, max_results, best_results.size(),
                worst_selected_cost)) {
            search_top_numeric_dags(
                root, pending, pending_set, current_choices, chosen_count + 1, size_bindings, next_cost,
                next_step_lb_cost, next_step_lb_size, max_results, best_results, worst_selected_cost, visited_buffer,
                stack_buffer);
        }

        // Backtrack
        for (int i = 0; i < added_children_count; ++i) {
            Id child_id = pending.back();
            pending.pop_back();
            pending_set[child_id] = 0;
        }
        // Undo the selection
        current_choices[current] = nullptr;
    }

    // Backtrack
    pending.push_back(current);
    std::swap(pending[pending.size() - 1], pending[original_index]);
    pending_set[current] = 1;
}

void Extractor::record_symbolic_result(
    Id root, const std::vector<const ENode *> &current_choices, const SymbolicCost &current_cost,
    std::vector<SymbolicSearchResult> &results) const {
    results.emplace_back(current_cost, convert_to_map(current_choices, {root}));
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
        record_symbolic_result(root, current_choices, current_cost, results);
        return;
    }

    if (nodes_visited >= node_visit_limit) {
        if (enable_logging) {
            std::cout << "[Extractor] Node visit limit reached, stopping search." << std::endl;
        }
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
            if (current_choices[child_root] == nullptr && !pending_set[child_root]) {
                pending.push_back(child_root);
                pending_set[child_root] = 1;
                added_children_count++;
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

    static thread_local size_t marker = 0;
    if (++marker == 0) {
        std::fill(visited_buffer.begin(), visited_buffer.end(), 0);
        marker = 1;
    }

    while (!stack_buffer.empty()) {
        Id node = stack_buffer.back();
        stack_buffer.pop_back();

        if (node == current_class) {
            return true;
        }

        if (visited_buffer[node] != marker) {
            visited_buffer[node] = marker;
            const ENode *chosen = current_choices[node];
            if (chosen) {
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
