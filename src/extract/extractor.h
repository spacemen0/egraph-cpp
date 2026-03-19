#pragma once

#include "cost_storage.h"
#include "e_graph.h"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ExtractionResult {
    Cost cost;
    Expression expr;
};

class Extractor {
  public:
    explicit Extractor(EGraph &egraph);

    ExtractionResult extract(Id class_id) const;
    ExtractionResult extract(Id class_id, const SizeBindings &size_bindings) const;
    std::vector<ExtractionResult> extract_symbolic(Id class_id) const;

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
    std::optional<NumericSearchResult>
    find_best_numeric_dag(Id root_class_id, const SizeBindings *size_bindings = nullptr) const;
    std::vector<SymbolicSearchResult> find_symbolic_dags(Id root_class_id) const;
    void search_best_numeric_dag(
        const std::vector<PendingClass> &pending, std::unordered_map<Id, const ENode *> &current_choices,
        const SizeBindings *size_bindings, double current_cost, double &best_cost,
        std::unordered_map<Id, const ENode *> &best_choices) const;
    void search_symbolic_dags(
        const std::vector<PendingClass> &pending, std::unordered_map<Id, const ENode *> &current_choices,
        SymbolicCost &current_cost, std::vector<SymbolicSearchResult> &results) const;

    Expression build_expression(
        Id class_id, const std::unordered_map<Id, const ENode *> &choices, std::unordered_set<Id> &visiting) const;
};
