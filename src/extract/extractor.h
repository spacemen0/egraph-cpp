#pragma once

#include "cost_storage.h"
#include "e_graph.h"
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ExtractionResult {
    Cost cost;
    Expression expr;
};

class Extractor {
  public:
    explicit Extractor(EGraph &egraph, CostStorage &cost_storage, bool enable_logging = false);

    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings = {}) const;
    std::vector<ExtractionResult>
    extract(Id class_id, size_t max_results, const SizeBindings &size_bindings = {}) const;

    ExtractionResult greedy_extract(Id class_id, const SizeBindings &size_bindings = {}) const;

    std::vector<ExtractionResult> extract_symbolic(Id class_id, bool build_expressions = true) const;
    bool collect_selected_nodes_for_binding(
        const std::vector<Id> &roots, const SizeBindings &size_bindings, size_t max_results,
        std::unordered_map<Id, std::unordered_set<const ENode *>> &selected_choices) const;

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
    CostStorage &cost_storage;
    bool enable_logging = false;
    mutable size_t nodes_visited = 0;
    size_t max_depth = 40;

    mutable std::unordered_map<Id, double> greedy_costs;     // Tree cost (Heuristic)
    mutable std::unordered_map<Id, double> cost_lower_bound; // Max-path cost (Safe LB)
    mutable std::unordered_map<Id, double> size_lower_bound; // Max-path size (Safe LB)
    mutable std::unordered_map<Id, const ENode *> greedy_choices;

    void compute_greedy_costs(const SizeBindings *size_bindings) const;

    std::vector<NumericSearchResult>
    find_top_numeric_dags(Id root_class_id, size_t max_results, const SizeBindings *size_bindings = nullptr) const;

    std::vector<SymbolicSearchResult> find_symbolic_dags(Id root_class_id) const;

    bool creates_cycle(
        Id current_class, const ENode *candidate, const std::vector<const ENode *> &current_choices,
        std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const;

    void search_top_numeric_dags(
        Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set,
        std::vector<const ENode *> &current_choices, size_t chosen_count, const SizeBindings *size_bindings,
        double current_cost, double current_pending_lb_cost, double current_pending_lb_size, size_t max_results,
        std::vector<NumericSearchResult> &best_results, double &worst_selected_cost,
        std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const;

    void search_symbolic_dags(
        Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set,
        std::vector<const ENode *> &current_choices, size_t chosen_count, const SymbolicCost &current_cost,
        std::vector<SymbolicSearchResult> &results, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer,
        const std::unordered_map<const ENode *, Cost> &node_costs) const;

    std::unordered_map<Id, const ENode *>
    convert_to_map(const std::vector<const ENode *> &choices, const std::vector<Id> &roots) const;

    Expression build_expression(
        Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const;
};
