#pragma once
#include "e_graph.h"
#include "cost_storage.h"

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
    EGraph &egraph;
    CostStorage &cost_storage;
    Expression build_expression(Id class_id) const;
};
