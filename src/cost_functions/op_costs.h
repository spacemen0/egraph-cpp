#pragma once
#include "basic_types.h"
#include "e_graph.h"
#include "e_node.h"
#include "types.h"

std::string size_to_symbol(const Size &size);

Cost compute_add_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_mul_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_tr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_inv_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_minus_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_qr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_lu_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_llt_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_utu_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_get_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_sol_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_solr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_scale_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_det_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_log_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_gemm_nn_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_syrk_n_syrk_t_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_trsm_ln_group_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_potrf_l_potrf_u_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_geqrf_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_trtri_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_gemv_n_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_gemv_t_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_orgqr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
Cost compute_ormqr_cost(Op op, const ENode &node, const EGraph &egraph, const SizeBindings *size_bindings);
