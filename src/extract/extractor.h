#pragma once

#include "e_graph.h"
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ExtractionResult {
    Cost cost;
    Expression expr;
    std::vector<Id> execution_order;
    std::unordered_map<Id, const ENode*> choices;
};

class Extractor {
  public:
    explicit Extractor(
        EGraph &egraph, bool enable_logging = false, size_t max_depth = 40,
        size_t node_visit_limit = 10000000);

    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings = {}) const;
    std::vector<ExtractionResult>
    extract(Id class_id, size_t max_results, const SizeBindings &size_bindings = {}) const;

    ExtractionResult tree_extract(Id class_id, const SizeBindings &size_bindings = {}) const;

    ExtractionResult ilp_extract(Id class_id, const SizeBindings &size_bindings = {}) const;

    std::vector<ExtractionResult> extract_symbolic(Id class_id, bool build_expressions = true) const;
    bool collect_selected_nodes_for_binding(
        const std::vector<Id> &roots, const SizeBindings &size_bindings, size_t max_results,
        std::unordered_map<Id, std::unordered_set<const ENode *>> &selected_choices) const;
    void reset() const;

  private:
    struct NumericSearchResult {
        double cost;
        std::unordered_map<Id, const ENode *> choices;
    };

    struct SymbolicSearchResult {
        SymbolicCost cost;
        std::unordered_map<Id, const ENode *> choices;
    };

    EGraph &egraph;
    bool enable_logging = false;
    mutable size_t nodes_visited = 0;
    size_t max_depth;
    size_t node_visit_limit;

    mutable std::unordered_map<Id, double> tree_cost;                       // Tree cost (Heuristic)
    mutable std::unordered_map<Id, double> minimal_possible_sub_tree_costs; // Max-path cost (Safe LB)
    mutable std::unordered_map<Id, size_t> minimal_possible_sizes;          // Max-path size (Safe LB)
    mutable std::unordered_map<Id, const ENode *> greedy_choices;

    void initial_tree_search_pass(const SizeBindings *size_bindings) const;

    std::vector<NumericSearchResult>
    find_top_numeric_dags(Id root_class_id, size_t max_results, const SizeBindings *size_bindings = nullptr) const;

    std::vector<SymbolicSearchResult> find_symbolic_dags(Id root_class_id) const;

    bool creates_cycle(
        Id current_class, const ENode *candidate, const std::vector<const ENode *> &current_choices,
        std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const;

    void search_top_numeric_dags(
        Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set,
        std::vector<const ENode *> &current_choices, size_t chosen_count, const SizeBindings *size_bindings,
        double current_cost, size_t max_results, std::vector<NumericSearchResult> &best_results,
        double &worst_selected_cost, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const;

    void record_numeric_result(
        Id root, const std::vector<const ENode *> &current_choices, double current_cost, size_t max_results,
        std::vector<NumericSearchResult> &best_results, double &worst_selected_cost) const;

    bool should_prune_numeric_search(
        size_t chosen_count, double current_cost, double pending_lb_cost, size_t pending_lb_size, size_t max_results,
        size_t best_results_count, double worst_selected_cost) const;

    struct Candidate {
        const ENode *node;
        double local_cost;
        double tree_cost_heuristic;
        double minimal_possible_cost;
        size_t minimal_possible_sub_treesize;
    };

    std::vector<Candidate> evaluate_node_candidates(Id eclass_id, const SizeBindings *size_bindings) const;

    void search_symbolic_dags(
        Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set,
        std::vector<const ENode *> &current_choices, size_t chosen_count, const SymbolicCost &current_cost,
        std::vector<SymbolicSearchResult> &results, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer,
        const std::unordered_map<const ENode *, Cost> &node_costs) const;

    void record_symbolic_result(
        Id root, const std::vector<const ENode *> &current_choices, const SymbolicCost &current_cost,
        std::vector<SymbolicSearchResult> &results) const;

    std::unordered_map<Id, const ENode *>
    convert_to_map(const std::vector<const ENode *> &choices, const std::vector<Id> &roots) const;

    Expression build_expression(
        Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const;
    std::vector<Id> build_execution_order(Id class_id, const std::unordered_map<Id, const ENode *> &choices) const;
};
