#pragma once
#include "basic_types.h"
#include "op_registry.h"
#include "types.h"
#include <vector>

namespace egraph {

Cost compute_add_cost(Op op, const std::vector<Operand> &operands);
Cost compute_mul_cost(Op op, const std::vector<Operand> &operands);
Cost compute_tr_cost(Op op, const std::vector<Operand> &operands);
Cost compute_inv_cost(Op op, const std::vector<Operand> &operands);
Cost compute_minus_cost(Op op, const std::vector<Operand> &operands);
Cost compute_qr_cost(Op op, const std::vector<Operand> &operands);
Cost compute_lu_cost(Op op, const std::vector<Operand> &operands);
Cost compute_cholel_cost(Op op, const std::vector<Operand> &operands);
Cost compute_choleu_cost(Op op, const std::vector<Operand> &operands);
Cost compute_get_cost(Op op, const std::vector<Operand> &operands);
Cost compute_scale_cost(Op op, const std::vector<Operand> &operands);
Cost compute_gemm_nn_group_cost(Op op, const std::vector<Operand> &operands);
Cost compute_syrk_n_syrk_t_cost(Op op, const std::vector<Operand> &operands);
Cost compute_trsm_ln_group_cost(Op op, const std::vector<Operand> &operands);
Cost compute_trmm_ln_group_cost(Op op, const std::vector<Operand> &operands);
Cost compute_symm_group_cost(Op op, const std::vector<Operand> &operands);
Cost compute_potrf_l_potrf_u_cost(Op op, const std::vector<Operand> &operands);
Cost compute_geqrf_cost(Op op, const std::vector<Operand> &operands);
Cost compute_trtri_cost(Op op, const std::vector<Operand> &operands);
Cost compute_gemv_n_cost(Op op, const std::vector<Operand> &operands);
Cost compute_gemv_t_cost(Op op, const std::vector<Operand> &operands);
Cost compute_orgqr_cost(Op op, const std::vector<Operand> &operands);
Cost compute_ormqr_cost(Op op, const std::vector<Operand> &operands);
Cost compute_axpy_cost(Op op, const std::vector<Operand> &operands);

} // namespace egraph
