#pragma once
#include "e_graph.h"
#include "cost_storage.h"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ExtractionResult
{
    Cost cost;
    Expression expr;
};

class Extractor
{
public:
    explicit Extractor(EGraph &egraph);

    ExtractionResult extract(Id class_id) const;

private:
    struct PendingClass
    {
        Id class_id;
        std::unordered_set<Id> ancestors;
    };

    struct SearchResult
    {
        double cost;
        std::unordered_map<Id, const ENode *> choices;
    };

    EGraph &egraph;
    CostStorage &cost_storage;
    std::optional<SearchResult> find_best_numeric_dag(Id root_class_id) const;
    void search_best_numeric_dag(
        std::vector<PendingClass> &pending,
        std::unordered_map<Id, const ENode *> &current_choices,
        double current_cost,
        double &best_cost,
        std::unordered_map<Id, const ENode *> &best_choices) const;
    Expression build_expression(
        Id class_id,
        const std::unordered_map<Id, const ENode *> &choices,
        std::unordered_set<Id> &visiting) const;
};
