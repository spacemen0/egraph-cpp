#pragma once
#include "analysis.h"

class EGraph;
class ENode;

void enforce_hierarchy(MatrixProperty &property);
AnalysisData analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op);
