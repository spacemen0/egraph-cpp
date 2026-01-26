#pragma once
#include "types.h"
#include "e_node.h"

class EGraph;

class MatrixAnalysis
{
public:
    static AnalysisData make(const EGraph &egraph, const ENode &node);
    static void merge(AnalysisData &data1, const AnalysisData &data2);
};
