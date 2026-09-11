#include "extractor.h"
#include "basic_types.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace egraph {
namespace {
constexpr size_t kExtractorProgressLogEvery = 1000000;
} // namespace

bool Extractor::is_unique_result(
    const std::vector<NumericSearchResult> &best_results, const std::unordered_map<Id, const ENode *> &choices_map) {
    for (const auto &existing : best_results) {
        if (existing.choices == choices_map) {
            return false;
        }
    }
    return true;
}

Extractor::Extractor(EGraph &egraph, const EGraphConfig &config)
    : egraph(egraph), enable_logging(config.enable_logging), max_depth(config.extractor.max_depth),
      node_visit_limit(config.extractor.node_visit_limit) {}

void Extractor::reset() const {
    tree_cost.clear();
    min_local_cost.clear();
    tree_choices.clear();
    nodes_visited = 0;
}

void Extractor::search_numeric_dags(
    Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set, std::vector<const ENode *> &current_choices,
    double current_g, double pending_min_local_sum, std::vector<NumericSearchResult> &results, double &worst_cost,
    size_t max_results, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer,
    const SizeBindings *size_bindings) const {

    nodes_visited++;
    if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
        std::cout << "[Extractor] Progress: visited=" << nodes_visited << ", pending=" << pending.size()
                  << ", best_results=" << results.size() << std::endl;
    }

    if (pending.empty()) {
        auto choices_map = convert_to_map(current_choices, {root});
        if (is_unique_result(results, choices_map)) {
            auto it = std::lower_bound(
                results.begin(), results.end(), current_g, [](const NumericSearchResult &r, double val) {
                return r.cost < val;
            });
            results.insert(it, NumericSearchResult{current_g, std::move(choices_map)});
            if (results.size() > max_results) {
                results.pop_back();
            }
            if (results.size() >= max_results) {
                worst_cost = results.back().cost;
            }
        }
        return;
    }

    if (nodes_visited >= node_visit_limit) {
        if (enable_logging) {
            std::cout << "[Extractor] Node visit limit reached during numeric search, stopping." << std::endl;
        }
        return;
    }

    Id current = pending.back();
    pending.pop_back();
    pending_set[current] = 0;
    double current_min_local = min_local_cost.contains(current) ? min_local_cost.at(current) : 0.0;
    double remaining_min_local = pending_min_local_sum - current_min_local;

    const auto &class_nodes = egraph.get_class_nodes(current);
    std::vector<const ENode *> candidate_nodes = class_nodes;
    const ENode *preferred = tree_choices.contains(current) ? tree_choices.at(current) : nullptr;
    if (preferred && candidate_nodes.size() > 1) {
        auto it = std::find(candidate_nodes.begin(), candidate_nodes.end(), preferred);
        if (it != candidate_nodes.end()) {
            std::swap(*it, candidate_nodes.front());
        }
    }

    for (const ENode *node : candidate_nodes) {
        Cost local_c = node->compute_local_cost(egraph, size_bindings);
        if (!std::holds_alternative<double>(local_c)) {
            continue;
        }
        double local_val = std::get<double>(local_c);
        if (local_val == std::numeric_limits<double>::infinity()) {
            continue;
        }

        double next_g = current_g + local_val;
        if (next_g + remaining_min_local >= worst_cost) {
            continue;
        }

        if (creates_cycle(current, node, current_choices, visited_buffer, stack_buffer)) {
            continue;
        }

        current_choices[current] = node;

        int added_children_count = 0;
        double added_min_local = 0.0;
        for (Id child : node->get_children()) {
            Id child_root = egraph.find_class_id(child);
            if (current_choices[child_root] == nullptr && !pending_set[child_root]) {
                pending.push_back(child_root);
                pending_set[child_root] = 1;
                added_children_count++;
                if (min_local_cost.contains(child_root)) {
                    added_min_local += min_local_cost.at(child_root);
                }
            }
        }

        search_numeric_dags(
            root, pending, pending_set, current_choices, next_g, remaining_min_local + added_min_local, results,
            worst_cost, max_results, visited_buffer, stack_buffer, size_bindings);

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

std::vector<Extractor::NumericSearchResult>
Extractor::find_top_numeric_dags(Id root_class_id, size_t max_results, const SizeBindings *size_bindings) const {
    if (max_results == 0) {
        return {};
    }

    Id root = egraph.find_class_id(root_class_id);

    initial_analysis_pass(size_bindings);

    if (tree_cost[root] == std::numeric_limits<double>::infinity()) {
        return {};
    }

    Id max_id = 0;
    for (Id id : egraph.get_all_class_ids()) {
        max_id = std::max(max_id, id);
    }

    nodes_visited = 0;
    std::vector<size_t> visited_buffer(max_id + 1, 0);
    std::vector<Id> stack_buffer;
    std::vector<const ENode *> current_choices(max_id + 1, nullptr);
    std::vector<size_t> pending_set(max_id + 1, 0);
    std::vector<Id> pending = {root};
    pending_set[root] = 1;

    std::vector<NumericSearchResult> best_results;
    double worst_cost = std::numeric_limits<double>::infinity();

    // Seed with tree extraction result to establish an immediate upper bound
    std::unordered_map<Id, const ENode *> seed_map;
    std::vector<Id> seed_stack = {root};
    while (!seed_stack.empty()) {
        Id current = seed_stack.back();
        seed_stack.pop_back();

        if (seed_map.contains(current)) {
            continue;
        }
        // simply use the best tree choice for each class
        auto it = tree_choices.find(current);
        if (it != tree_choices.end() && it->second) {
            seed_map[current] = it->second;
            for (Id child : it->second->get_children()) {
                seed_stack.push_back(egraph.find_class_id(child));
            }
        }
    }
    if (!seed_map.empty()) {
        double seed_dag_cost = 0.0;
        for (const auto &[cls, node] : seed_map) {
            Cost c = node->compute_local_cost(egraph, size_bindings);
            if (std::holds_alternative<double>(c)) {
                seed_dag_cost += std::get<double>(c);
            }
        }
        best_results.push_back(NumericSearchResult{seed_dag_cost, std::move(seed_map)});
        if (best_results.size() >= max_results) {
            worst_cost = seed_dag_cost;
        }
    }

    double initial_min_local = min_local_cost.contains(root) ? min_local_cost.at(root) : 0.0;

    search_numeric_dags(
        root, pending, pending_set, current_choices, 0.0, initial_min_local, best_results, worst_cost, max_results,
        visited_buffer, stack_buffer, size_bindings);

    if (enable_logging) {
        std::cout << "[Extractor] Visited " << nodes_visited << " nodes during numeric extraction." << std::endl;
    }

    if (best_results.empty()) {
        throw std::runtime_error("Runtime error: no numeric DAG found for root class under supplied size bindings");
    }

    std::sort(
        best_results.begin(), best_results.end(), [](const NumericSearchResult &lhs, const NumericSearchResult &rhs) {
        return lhs.cost < rhs.cost;
    });

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

// this function does two things in one pass:
// 1. Compute the admissible DAG lower-bound cost for each e-class (used for greedy tie-breaking in tree extraction).
// 2. Compute the best tree cost and corresponding e-node for each e-class.
// 3. Compute the minimum local cost for each e-class (independent of children).
void Extractor::initial_analysis_pass(const SizeBindings *size_bindings) const {
    tree_cost.clear();
    min_local_cost.clear();
    tree_choices.clear();

    // Admissible DAG lower-bound cost for each e-class (used for greedy tie-breaking in tree extraction)
    std::unordered_map<Id, double> dag_costs_lower_bound;
    // dag costs lower bound but for the current chosen node in tree choices for each class
    std::unordered_map<Id, double> chosen_node_dag_cost_lower_bound;

    auto all_class_ids = egraph.get_all_class_ids();
    for (Id id : all_class_ids) {
        tree_cost[id] = std::numeric_limits<double>::infinity();
        dag_costs_lower_bound[id] = std::numeric_limits<double>::infinity();
        min_local_cost[id] = std::numeric_limits<double>::infinity();
        chosen_node_dag_cost_lower_bound[id] = std::numeric_limits<double>::infinity();
    }

    // compute minimum local cost per class (independent of children)
    for (Id class_id : all_class_ids) {
        for (const ENode *node : egraph.get_class_nodes(class_id)) {
            Cost local_cost = node->compute_local_cost(egraph, size_bindings);
            if (std::holds_alternative<double>(local_cost)) {
                double local_val = std::get<double>(local_cost);
                if (local_val < min_local_cost[class_id]) {
                    min_local_cost[class_id] = local_val;
                }
            }
        }
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
                double max_child_cost = 0;

                double node_tree_cost = local;
                bool children_incomplete = false;

                for (Id child : node->get_children()) {
                    Id child_root = egraph.find_class_id(child);

                    if (tree_cost[child_root] == std::numeric_limits<double>::infinity()) {
                        children_incomplete = true;
                        break;
                    }

                    max_child_cost = std::max(max_child_cost, dag_costs_lower_bound[child_root]);
                    node_tree_cost += tree_cost[child_root];
                }

                if (children_incomplete) {
                    continue;
                }

                double node_lb_cost = local + max_child_cost;

                // we have found a better lower bound for this class, but it is not always the node selected for the
                // best tree cost
                if (node_lb_cost < dag_costs_lower_bound[class_id]) {
                    dag_costs_lower_bound[class_id] = node_lb_cost;
                    changed = true;
                }

                // if tree cost is smaller than the current best, or if it's equal but the lower bound is smaller,
                // update the best choice
                if (node_tree_cost < tree_cost[class_id] ||
                    (node_tree_cost == tree_cost[class_id] &&
                     node_lb_cost < chosen_node_dag_cost_lower_bound[class_id])) {
                    tree_cost[class_id] = node_tree_cost;
                    chosen_node_dag_cost_lower_bound[class_id] = node_lb_cost;
                    tree_choices[class_id] = node;
                    changed = true;
                }
            }
        }
    }
}

ExtractionResult Extractor::tree_extract(Id class_id, const SizeBindings &size_bindings) const {
    Id root = egraph.find_class_id(class_id);
    initial_analysis_pass(size_bindings.empty() ? nullptr : &size_bindings);
    if (tree_cost.at(root) == std::numeric_limits<double>::infinity()) {
        throw std::runtime_error("Runtime error: no numeric DAG found for root class under supplied size bindings");
    }

    std::unordered_map<Id, const ENode *> reachable_choices;
    std::vector<Id> stack = {root};
    while (!stack.empty()) {
        Id curr = stack.back();
        stack.pop_back();

        if (reachable_choices.contains(curr)) {
            continue;
        }

        auto it = tree_choices.find(curr);
        if (it != tree_choices.end() && it->second) {
            reachable_choices[curr] = it->second;
            for (Id child : it->second->get_children()) {
                stack.push_back(egraph.find_class_id(child));
            }
        }
    }

    std::unordered_set<Id> visiting;
    return ExtractionResult{
        tree_cost.at(root), build_expression(root, reachable_choices, visiting),
        build_execution_order(root, reachable_choices), reachable_choices};
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

std::vector<Id>
Extractor::build_execution_order(Id class_id, const std::unordered_map<Id, const ENode *> &choices) const {
    Id root = egraph.find_class_id(class_id);
    std::vector<Id> execution_order;
    std::unordered_set<Id> visited;

    auto dfs = [&](auto &self, Id current_id) -> void {
        Id current = egraph.find_class_id(current_id);
        if (visited.count(current))
            return;
        visited.insert(current);

        auto it = choices.find(current);
        if (it != choices.end()) {
            for (Id child_id : it->second->get_children()) {
                self(self, child_id);
            }
        }
        execution_order.push_back(current);
    };

    dfs(dfs, root);
    return execution_order;
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
        results.push_back(
            {dag.cost, build_expression(class_id, dag.choices, visiting), build_execution_order(class_id, dag.choices),
             dag.choices});
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

    // Use a thread-local marker to avoid clearing the visited_buffer on every call
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
        Expression expr;
        if (build_expressions) {
            expr = build_expression(class_id, dag.choices, visiting);
        }
        results.push_back({dag.cost, expr, build_execution_order(class_id, dag.choices), dag.choices});
    }
    return results;
}

/// Collects the extracted nodes for the given roots and size bindings, storing them in selected_choices. Returns true
bool Extractor::collect_selected_nodes_for_binding(
    const std::vector<Id> &roots, const SizeBindings &size_bindings,
    std::unordered_map<Id, std::unordered_set<const ENode *>> &selected_choices) const {
    bool any_root_succeeded = false;

    for (Id root : roots) {
        try {
            auto result = tree_extract(root, size_bindings);
            any_root_succeeded = true;
            for (const auto &[class_id, node] : result.choices) {
                selected_choices[class_id].insert(node);
            }
        } catch (const std::exception &) {
            // Skip if no tree found for this root under these bindings
        }
    }

    return any_root_succeeded;
}

} // namespace egraph
