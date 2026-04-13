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
    explicit Extractor(EGraph &egraph, CostStorage &cost_storage);

    ExtractionResult extract(Id class_id) const;
    std::vector<ExtractionResult> extract(Id class_id, size_t max_results) const;
    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings) const;
    std::vector<ExtractionResult> extract(Id class_id, const SizeBindings &size_bindings, size_t max_results) const;
    std::vector<ExtractionResult> extract_symbolic(Id class_id) const;
    bool collect_selected_nodes_for_binding(
        const std::vector<Id> &roots, const SizeBindings &size_bindings, size_t max_results,
        std::unordered_map<Id, const ENode *> &selected_choices) const;

  private:
    struct PendingClass {
        Id class_id;
        std::unordered_set<Id> ancestors;
    };

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
    std::vector<NumericSearchResult>
    find_top_numeric_dags(Id root_class_id, size_t max_results, const SizeBindings *size_bindings = nullptr) const;
    std::vector<SymbolicSearchResult> find_symbolic_dags(Id root_class_id) const;
    void search_top_numeric_dags(
        const std::vector<PendingClass> &pending, std::unordered_map<Id, const ENode *> &current_choices,
        const SizeBindings *size_bindings, double current_cost, size_t max_results,
        std::vector<NumericSearchResult> &best_results, double &worst_selected_cost) const;
    void search_symbolic_dags(
        const std::vector<PendingClass> &pending, std::unordered_map<Id, const ENode *> &current_choices,
        SymbolicCost &current_cost, std::vector<SymbolicSearchResult> &results) const;

    Expression build_expression(
        Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const;
};
