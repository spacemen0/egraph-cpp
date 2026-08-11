#include "extractor.h"
#include "basic_types.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <queue>
#include <unordered_set>

namespace {
constexpr size_t kExtractorProgressLogEvery = 1000000;

struct ChoiceNode {
    Id class_id;
    const ENode *node;
    std::shared_ptr<const ChoiceNode> parent;
};

struct AStarSearchNode {
    // exact cost of selected nodes so far
    double g_cost;
    // lower bound of cost of remaining nodes
    double h_cost;
    std::shared_ptr<const ChoiceNode> choices_head;
    std::vector<Id> pending;

    double f_cost() const { return g_cost + h_cost; }

    bool operator>(const AStarSearchNode &other) const {
        if (std::abs(f_cost() - other.f_cost()) > 1e-9) {
            return f_cost() > other.f_cost();
        }
        if (std::abs(g_cost - other.g_cost) > 1e-9) {
            return g_cost > other.g_cost;
        }
        return pending.size() > other.pending.size();
    }
};

static std::vector<const ENode *> choices_to_vector(const std::shared_ptr<const ChoiceNode> &head, size_t max_id) {
    std::vector<const ENode *> vec(max_id + 1, nullptr);
    for (auto curr = head; curr != nullptr; curr = curr->parent) {
        if (curr->class_id <= max_id) {
            vec[curr->class_id] = curr->node;
        }
    }
    return vec;
}
} // namespace

Extractor::Extractor(EGraph &egraph, const ExtractorConfig &config)
    : egraph(egraph), enable_logging(config.enable_logging), max_depth(config.max_depth),
      node_visit_limit(config.node_visit_limit) {}

Extractor::Extractor(EGraph &egraph, const EGraphConfig &config) : Extractor(egraph, config.extractor) {}

void Extractor::reset() const {
    tree_cost.clear();
    minimal_possible_sub_tree_costs.clear();
    min_local_cost.clear();
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

    initial_tree_search_pass(size_bindings);

    if (tree_cost[root] == std::numeric_limits<double>::infinity()) {
        return {};
    }

    Id max_id = 0;
    for (Id id : egraph.get_all_class_ids()) {
        max_id = std::max(max_id, id);
    }

    // First node is the node with lowest f_cost
    std::priority_queue<AStarSearchNode, std::vector<AStarSearchNode>, std::greater<AStarSearchNode>> pq;

    AStarSearchNode start_node;
    start_node.g_cost = 0.0;
    start_node.h_cost = minimal_possible_sub_tree_costs.at(root);
    start_node.choices_head = nullptr;
    start_node.pending.push_back(root);

    pq.push(start_node);

    nodes_visited = 0;
    std::vector<size_t> visited_buffer(max_id + 1, 0);
    std::vector<Id> stack_buffer;

    std::vector<NumericSearchResult> best_results;

    while (!pq.empty() && best_results.size() < max_results) {

        // Pop the first node
        AStarSearchNode top = std::move(const_cast<AStarSearchNode &>(pq.top()));
        pq.pop();
        nodes_visited++;

        if (enable_logging && (nodes_visited % kExtractorProgressLogEvery == 0)) {
            std::cout << "[Extractor] Progress (A*): visited=" << nodes_visited << ", pending=" << top.pending.size()
                      << ", best_results=" << best_results.size() << std::endl;
        }

        auto choices_vec = choices_to_vector(top.choices_head, max_id);

        if (top.pending.empty()) {
            bool is_unique = true;
            for (const auto &existing : best_results) {
                if (std::abs(existing.cost - top.g_cost) < 1e-9) {
                    is_unique = false;
                    break;
                }
            }
            if (is_unique) {
                best_results.push_back(NumericSearchResult{top.g_cost, convert_to_map(choices_vec, {root})});
            }
            continue;
        }

        if (nodes_visited >= node_visit_limit) {
            if (enable_logging) {
                std::cout << "[Extractor] Node visit limit reached during A* search, stopping." << std::endl;
            }
            break;
        }
        // decide which class to expand next: pick the one with the highest lower bound
        auto it = std::max_element(top.pending.begin(), top.pending.end(), [&](Id a, Id b) {
            return minimal_possible_sub_tree_costs.at(a) < minimal_possible_sub_tree_costs.at(b);
        });

        Id curr_class = *it;
        size_t orig_idx = std::distance(top.pending.begin(), it);
        std::swap(top.pending[orig_idx], top.pending.back());
        top.pending.pop_back();

        const auto &class_nodes = egraph.get_class_nodes(curr_class);
        for (const ENode *node : class_nodes) {
            Cost local_c = node->compute_local_cost(egraph, size_bindings);
            if (!std::holds_alternative<double>(local_c))
                continue;

            double local_val = std::get<double>(local_c);
            if (local_val == std::numeric_limits<double>::infinity())
                continue;

            if (creates_cycle(curr_class, node, choices_vec, visited_buffer, stack_buffer)) {
                continue;
            }

            AStarSearchNode next_state;
            next_state.choices_head = std::make_shared<ChoiceNode>(ChoiceNode{curr_class, node, top.choices_head});
            next_state.g_cost = top.g_cost + local_val;
            next_state.pending = top.pending;

            for (Id child : node->get_children()) {
                Id child_root = egraph.find_class_id(child);
                if (choices_vec[child_root] == nullptr) {
                    bool already_pending = false;
                    for (Id p : next_state.pending) {
                        if (p == child_root) {
                            already_pending = true;
                            break;
                        }
                    }
                    if (!already_pending) {
                        next_state.pending.push_back(child_root);
                    }
                }
            }

            double max_pending_lb = 0;
            for (Id p : next_state.pending) {
                max_pending_lb = std::max(max_pending_lb, minimal_possible_sub_tree_costs.at(p));
            }
            next_state.h_cost = max_pending_lb;

            pq.push(std::move(next_state));
        }
    }

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

void Extractor::initial_tree_search_pass(const SizeBindings *size_bindings) const {
    tree_cost.clear();
    minimal_possible_sub_tree_costs.clear();
    min_local_cost.clear();
    minimal_possible_sizes.clear();
    greedy_choices.clear();

    auto all_class_ids = egraph.get_all_class_ids();
    for (Id id : all_class_ids) {
        tree_cost[id] = std::numeric_limits<double>::infinity();
        minimal_possible_sub_tree_costs[id] = std::numeric_limits<double>::infinity();
        min_local_cost[id] = std::numeric_limits<double>::infinity();
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
                min_local_cost[class_id] = std::min(min_local_cost[class_id], local);

                // Lower bounds for dag cost and size
                double max_child_cost = 0;
                size_t max_child_size = 0;
                bool children_incomplete_for_lb = false;

                // Tree cost
                double current_node_greedy_cost = local;
                bool children_incomplete_for_greedy = false;

                for (Id child : node->get_children()) {
                    Id child_root = egraph.find_class_id(child);

                    if (minimal_possible_sub_tree_costs[child_root] == std::numeric_limits<double>::infinity()) {
                        children_incomplete_for_lb = true;
                    } else {

                        // use std::max because this is at least the cost of all children, even all other children are
                        // free by sharing.
                        max_child_cost = std::max(max_child_cost, minimal_possible_sub_tree_costs[child_root]);
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
                    if (node_lb_cost < minimal_possible_sub_tree_costs[class_id]) {
                        minimal_possible_sub_tree_costs[class_id] = node_lb_cost;
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
    return ExtractionResult{
        tree_cost.at(root), build_expression(root, greedy_choices, visiting),
        build_execution_order(root, greedy_choices), greedy_choices};
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
