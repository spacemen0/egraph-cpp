#pragma once
#include "e_graph.h"
#include <unordered_map>

struct ExtractionResult
{
    double cost;
    Expression expr;
};

class Extractor
{
public:
    explicit Extractor(const EGraph &egraph);

    ExtractionResult extract(Id class_id) const;

private:
    const EGraph &egraph;
    std::unordered_map<Id, double> costs;
    std::unordered_map<Id, const ENode *> best_nodes;

    void calculate_costs();
    double get_node_cost(const ENode &node) const;
    Expression build_expression(Id class_id) const;
};
