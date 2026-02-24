#pragma once
#include "e_graph.h"
#include "cost_storage.h"

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
    CostStorage cost_storage;
    Expression build_expression(Id class_id) const;
};
