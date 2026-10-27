#pragma once
#include "e_node.h"
#include "op_registry.h"
#include "property_table.h"

namespace egraph {
class EGraph;

namespace MatrixAnalysis {
AnalysisData make(const EGraph &egraph, const ENode &node);
bool merge(AnalysisData &data1, const AnalysisData &data2);
void enforce_hierarchy(MatrixProperty &property);
AnalysisData analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op);
}; // namespace MatrixAnalysis
AnalysisData analyze_add(const std::vector<Operand> &operands);
AnalysisData analyze_mul(const std::vector<Operand> &operands);
AnalysisData analyze_transpose(const std::vector<Operand> &operands);
AnalysisData analyze_invert(const std::vector<Operand> &operands);
AnalysisData analyze_minus(const std::vector<Operand> &operands);
AnalysisData analyze_geqrf(const std::vector<Operand> &operands);
AnalysisData analyze_qr(const std::vector<Operand> &operands);
AnalysisData analyze_lu(const std::vector<Operand> &operands);
AnalysisData analyze_get(const std::vector<Operand> &operands);
AnalysisData analyze_potrf_l(const std::vector<Operand> &operands);
AnalysisData analyze_cholel(const std::vector<Operand> &operands);
AnalysisData analyze_gemm_nn(const std::vector<Operand> &operands);
AnalysisData analyze_gemm_tn(const std::vector<Operand> &operands);
AnalysisData analyze_gemm_nt(const std::vector<Operand> &operands);
AnalysisData analyze_gemm_tt(const std::vector<Operand> &operands);
AnalysisData analyze_syrk_n(const std::vector<Operand> &operands);
AnalysisData analyze_syrk_t(const std::vector<Operand> &operands);
AnalysisData analyze_trmm_ln(const std::vector<Operand> &operands);
AnalysisData analyze_trmm_lt(const std::vector<Operand> &operands);
AnalysisData analyze_trmm_rn(const std::vector<Operand> &operands);
AnalysisData analyze_trmm_rt(const std::vector<Operand> &operands);
AnalysisData analyze_symm_l(const std::vector<Operand> &operands);
AnalysisData analyze_symm_r(const std::vector<Operand> &operands);
AnalysisData analyze_trsm_ln(const std::vector<Operand> &operands);
AnalysisData analyze_trsm_lt(const std::vector<Operand> &operands);
AnalysisData analyze_trsm_rn(const std::vector<Operand> &operands);
AnalysisData analyze_trsm_rt(const std::vector<Operand> &operands);
AnalysisData analyze_potrf_u(const std::vector<Operand> &operands);
AnalysisData analyze_choleu(const std::vector<Operand> &operands);
AnalysisData analyze_trtri(const std::vector<Operand> &operands);
AnalysisData analyze_gemv_n(const std::vector<Operand> &operands);
AnalysisData analyze_gemv_t(const std::vector<Operand> &operands);
AnalysisData analyze_orgqr(const std::vector<Operand> &operands);
AnalysisData analyze_ormqr_ln(const std::vector<Operand> &operands);
AnalysisData analyze_ormqr_lt(const std::vector<Operand> &operands);
AnalysisData analyze_ormqr_rn(const std::vector<Operand> &operands);
AnalysisData analyze_ormqr_rt(const std::vector<Operand> &operands);
AnalysisData analyze_axpy(const std::vector<Operand> &operands);
AnalysisData analyze_scale(const std::vector<Operand> &operands);
} // namespace egraph
