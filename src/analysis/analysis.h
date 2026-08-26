#pragma once
#include "e_node.h"
#include "property_table.h"


namespace egraph {
class EGraph;

namespace MatrixAnalysis {
AnalysisData make(const EGraph &egraph, const ENode &node);
bool merge(AnalysisData &data1, const AnalysisData &data2);
void enforce_hierarchy(MatrixProperty &property);
AnalysisData analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op);
}; // namespace MatrixAnalysis

} // namespace egraph
