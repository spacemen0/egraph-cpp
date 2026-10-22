#pragma once

#include "e_graph.h"
#include "egraph_config.h"
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace egraph {
struct ExtractionResult {
    Cost cost;
    Expression expr;
    std::vector<Id> execution_order;
    std::unordered_map<Id, const ENode *> choices;
};

class Extractor {
  public:
    explicit Extractor(EGraph &egraph, EGraphConfig config = EGraphConfig())
        : egraph(egraph), config(std::move(config)) {}

    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings = {}, size_t visit_limit = 0) const;
    std::vector<ExtractionResult>
    extract(Id class_id, size_t max_results, const SizeBindings &size_bindings = {}, size_t visit_limit = 0) const;

    ExtractionResult tree_extract(Id class_id, const SizeBindings &size_bindings = {}) const;

    void collect_selected_nodes_for_binding(
        const std::vector<Id> &roots, const SizeBindings &size_bindings,
        std::unordered_map<Id, std::unordered_set<const ENode *>> &selected_choices, bool use_dag_extract,
        size_t dag_visit_limit) const;
    void reset() const;

    void set_config(const EGraphConfig &cfg) { config = cfg; }
    const EGraphConfig &get_config() const { return config; }

    std::vector<Id> build_execution_order(Id class_id, const std::unordered_map<Id, const ENode *> &choices) const;
    std::vector<Id>
    build_execution_order(const std::vector<Id> &roots, const std::unordered_map<Id, const ENode *> &choices) const;

  private:
    struct NumericSearchResult {
        double cost;
        std::unordered_map<Id, const ENode *> choices;
    };

    static bool is_unique_result(
        const std::vector<NumericSearchResult> &best_results, const std::unordered_map<Id, const ENode *> &choices_map);

    EGraph &egraph;
    EGraphConfig config;
    mutable size_t nodes_visited = 0;

    // Best tree extraction cost for each e-class: local_cost + sum(child_tree_costs)
    mutable std::vector<double> tree_cost;
    // Best e-node choice minimizing tree_cost
    mutable std::vector<const ENode *> tree_choices;
    // Admissible DAG lower-bound cost for each e-node
    mutable std::unordered_map<const ENode *, double> node_dag_lower_bound;

    void initial_analysis_pass(const SizeBindings *size_bindings) const;

    std::pair<double, std::unordered_map<Id, const ENode *>>
    evaluate_dag(Id root, const std::vector<const ENode *> &choices, const SizeBindings *size_bindings) const;

    NumericSearchResult refine_seed_dag(Id root, const SizeBindings *size_bindings) const;

    std::vector<NumericSearchResult> find_top_numeric_dags(
        Id root_class_id, size_t max_results, const SizeBindings *size_bindings = nullptr,
        size_t custom_visit_limit = 0) const;

    void search_numeric_dags(
        Id root, std::vector<Id> &pending, std::vector<size_t> &pending_set,
        std::vector<const ENode *> &current_choices, double current_cost, std::vector<NumericSearchResult> &results,
        double &worst_cost, size_t max_results, std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer,
        const SizeBindings *size_bindings, size_t effective_visit_limit) const;

    bool creates_cycle(
        Id current_class, const ENode *candidate, const std::vector<const ENode *> &current_choices,
        std::vector<size_t> &visited_buffer, std::vector<Id> &stack_buffer) const;

    std::unordered_map<Id, const ENode *>
    convert_to_map(const std::vector<const ENode *> &choices, const std::vector<Id> &roots) const;

    Expression build_expression(
        Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const;
};

} // namespace egraph
