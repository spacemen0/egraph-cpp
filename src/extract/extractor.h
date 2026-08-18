#pragma once

#include "e_graph.h"
#include "egraph_config.h"
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ExtractionResult {
    Cost cost;
    Expression expr;
    std::vector<Id> execution_order;
    std::unordered_map<Id, const ENode *> choices;
};

class Extractor {
  public:
    explicit Extractor(EGraph &egraph, const EGraphConfig &config = EGraphConfig());

    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings = {}) const;
    std::vector<ExtractionResult>
    extract(Id class_id, size_t max_results, const SizeBindings &size_bindings = {}) const;

    ExtractionResult tree_extract(Id class_id, const SizeBindings &size_bindings = {}) const;

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

    static bool is_unique_result(
        const std::vector<NumericSearchResult> &best_results,
        const std::unordered_map<Id, const ENode *> &choices_map);

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
