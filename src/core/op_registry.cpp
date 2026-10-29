#include "op_registry.h"
#include "analysis.h"
#include "errors.h"
#include "evaluator.h"
#include "op_costs.h"
#include "utils.h"

namespace egraph {

bool is_kernel_op(Op op) { return OpRegistry::instance().is_kernel(op); }

OpRegistry &OpRegistry::instance() {
    static OpRegistry registry;
    return registry;
}

OpRegistry::OpRegistry() { init_builtins(); }

void OpRegistry::init_builtins() { init_builtin_ops(*this); }

Op OpRegistry::register_op(
    const std::string &name, bool is_kernel, AnalyzeFn analyze, CostFn cost, EvalFn eval,
    std::optional<int> in_place_input) {
    if (auto it = name_to_op_.find(name); it != name_to_op_.end()) {
        std::string error_msg = "Operation already registered: " + name;
        throw InvalidOperationError(error_msg);
    }

    auto new_op = static_cast<Op>(ops.size());
    ops.push_back(
        OpDescriptor{
            .op = new_op,
            .name = name,
            .is_kernel = is_kernel,
            .analyze = std::move(analyze),
            .cost = std::move(cost),
            .eval = std::move(eval),
            .in_place_input = in_place_input,
        });
    name_to_op_[name] = new_op;
    return new_op;
}

Op OpRegistry::register_op(
    const std::string &name, bool is_kernel, AnalyzeFn analyze, CostFn cost, MatrixEvalFn eval,
    std::optional<int> in_place_input) {
    EvalFn wrapped = nullptr;
    if (eval) {
        wrapped = [fn = std::move(eval)](
            std::string_view op_name, const std::vector<Operand> &operands,
            const std::vector<const EvalValue *> &inputs, EvalValue &output) {
            std::vector<const MatrixNode *> mat_inputs;
            mat_inputs.reserve(inputs.size());
            for (const auto *in : inputs) {
                mat_inputs.push_back(&std::get<MatrixNode>(*in));
            }
            fn(op_name, operands, mat_inputs, std::get<MatrixNode>(output));
        };
    }
    return register_op(name, is_kernel, std::move(analyze), std::move(cost), std::move(wrapped), in_place_input);
}

bool OpRegistry::is_kernel(Op op) const {
    auto idx = static_cast<size_t>(op);
    if (idx < ops.size()) {
        return ops[idx].is_kernel;
    }
    return false;
}

std::string_view OpRegistry::name(Op op) {
    auto idx = static_cast<size_t>(op);
    if (idx < instance().ops.size()) {
        return instance().ops[idx].name;
    }
    return "UnknownOp";
}

std::optional<Op> OpRegistry::find_by_name(std::string_view name) const {
    if (auto it = name_to_op_.find(std::string(name)); it != name_to_op_.end()) {
        return it->second;
    }
    return std::nullopt;
}

Op OpRegistry::op(std::string_view name) {
    if (auto it = instance().find_by_name(name); it.has_value()) {
        return it.value();
    }
    throw InvalidOperationError("Operation not found in OpRegistry: " + std::string(name));
}

const OpDescriptor &OpRegistry::get(Op op) const {
    auto idx = static_cast<size_t>(op);
    if (idx < ops.size() && !ops[idx].name.empty()) {
        return ops[idx];
    }
    throw InvalidOperationError("Op not found in OpRegistry: " + std::to_string(static_cast<uint32_t>(op)));
}

bool OpRegistry::contains(Op op) const {
    auto idx = static_cast<size_t>(op);
    return idx < ops.size() && !ops[idx].name.empty();
}

bool OpRegistry::contains(std::string_view name) const { return name_to_op_.contains(std::string(name)); }

void init_operands(
    const EGraph &egraph, const Children &children, std::vector<Operand> &operands, const SizeBindings *bindings) {
    operands.reserve(children.size());

    for (Id child_id : children) {
        if (auto idx = get_int_from_eclass(egraph, child_id)) {
            operands.emplace_back(*idx);
        } else if (auto dbl = get_double_from_eclass(egraph, child_id)) {
            operands.emplace_back(*dbl);
        } else {
            bool is_symbolic_scalar = false;
            for (const auto &cnode : egraph.get_class_nodes(child_id)) {
                if (std::holds_alternative<ScalarExpr>(cnode->get_atom())) {
                    is_symbolic_scalar = true;
                    break;
                }
            }
            if (is_symbolic_scalar) {
                operands.emplace_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            const auto &class_data = egraph.get_class_analysis_data(child_id);
            if (auto prop = std::get_if<MatrixProperty>(&class_data.property)) {
                MatrixOperand mat;
                mat.property = *prop;
                if (bindings) {
                    Shape bound = bind_shape(prop->shape, *bindings);
                    if (!is_numeric(bound)) {
                        throw std::runtime_error("Cannot compute cost: missing size binding for symbolic dimension");
                    }
                    mat.rows = std::get<int>(bound.first);
                    mat.cols = std::get<int>(bound.second);
                    mat.property.shape = bound;
                } else {
                    if (auto r = std::get_if<int>(&prop->shape.first)) {
                        mat.rows = *r;
                    }
                    if (auto c = std::get_if<int>(&prop->shape.second)) {
                        mat.cols = *c;
                    }
                }
                operands.emplace_back(mat);
            } else if (auto tuple_prop = std::get_if<TupleProperty>(&class_data.property)) {
                TupleOperand tuple;
                tuple.reserve(tuple_prop->size());
                for (const auto &elem_prop : *tuple_prop) {
                    MatrixOperand elem;
                    elem.property = elem_prop;
                    if (bindings) {
                        Shape bound = bind_shape(elem_prop.shape, *bindings);
                        if (!is_numeric(bound)) {
                            throw std::runtime_error(
                                "Cannot compute cost: missing size binding for symbolic dimension");
                        }
                        elem.rows = std::get<int>(bound.first);
                        elem.cols = std::get<int>(bound.second);
                        elem.property.shape = bound;
                    } else {
                        if (auto r = std::get_if<int>(&elem_prop.shape.first)) {
                            elem.rows = *r;
                        }
                        if (auto c = std::get_if<int>(&elem_prop.shape.second)) {
                            elem.cols = *c;
                        }
                    }

                    tuple.push_back(elem);
                }
                operands.emplace_back(tuple);
            }
        }
    }
}

void init_builtin_ops(OpRegistry &registry) {
    registry.register_op("Add", false, analyze_add, compute_add_cost);
    registry.register_op("Mul", false, analyze_mul, compute_mul_cost);
    registry.register_op("Minus", false, analyze_minus, compute_minus_cost);
    registry.register_op("Tr", true, analyze_transpose, compute_tr_cost, eval_transpose);
    registry.register_op("Inv", false, analyze_invert, compute_inv_cost);
    registry.register_op("Get", false, analyze_get, compute_get_cost, eval_get);
    registry.register_op("Scale", true, analyze_scale, compute_scale_cost, eval_scale, 0);

    registry.register_op("QR", false, analyze_qr, compute_qr_cost);
    registry.register_op("LU", false, analyze_lu, compute_lu_cost);
    registry.register_op("CholeL", false, analyze_cholel, compute_cholel_cost);
    registry.register_op("CholeU", false, analyze_choleu, compute_choleu_cost);
    registry.register_op("Geqrf", true, analyze_geqrf, compute_geqrf_cost, eval_geqrf, 0);

    registry.register_op("Trtri", true, analyze_trtri, compute_trtri_cost, eval_trtri, 0);
    registry.register_op("Gemv_N", true, analyze_gemv_n, compute_gemv_n_cost, eval_gemv_group, 2);
    registry.register_op("Gemv_T", true, analyze_gemv_t, compute_gemv_t_cost, eval_gemv_group, 2);
    registry.register_op("Gemm_NN", true, analyze_gemm_nn, compute_gemm_nn_group_cost, eval_gemm_group, 2);
    registry.register_op("Gemm_TN", true, analyze_gemm_tn, compute_gemm_nn_group_cost, eval_gemm_group, 2);
    registry.register_op("Gemm_NT", true, analyze_gemm_nt, compute_gemm_nn_group_cost, eval_gemm_group, 2);
    registry.register_op("Gemm_TT", true, analyze_gemm_tt, compute_gemm_nn_group_cost, eval_gemm_group, 2);
    registry.register_op("Syrk_N", true, analyze_syrk_n, compute_syrk_n_syrk_t_cost, eval_syrk_group, 1);
    registry.register_op("Syrk_T", true, analyze_syrk_t, compute_syrk_n_syrk_t_cost, eval_syrk_group, 1);
    registry.register_op("Trmm_LN", true, analyze_trmm_ln, compute_trmm_ln_group_cost, eval_trmm_group, 1);
    registry.register_op("Trmm_LT", true, analyze_trmm_lt, compute_trmm_ln_group_cost, eval_trmm_group, 1);
    registry.register_op("Trmm_RN", true, analyze_trmm_rn, compute_trmm_ln_group_cost, eval_trmm_group, 1);
    registry.register_op("Trmm_RT", true, analyze_trmm_rt, compute_trmm_ln_group_cost, eval_trmm_group, 1);
    registry.register_op("Symm_L", true, analyze_symm_l, compute_symm_group_cost, eval_symm_group, 2);
    registry.register_op("Symm_R", true, analyze_symm_r, compute_symm_group_cost, eval_symm_group, 2);
    registry.register_op("Trsm_LN", true, analyze_trsm_ln, compute_trsm_ln_group_cost, eval_trsm_group, 1);
    registry.register_op("Trsm_LT", true, analyze_trsm_lt, compute_trsm_ln_group_cost, eval_trsm_group, 1);
    registry.register_op("Trsm_RN", true, analyze_trsm_rn, compute_trsm_ln_group_cost, eval_trsm_group, 1);
    registry.register_op("Trsm_RT", true, analyze_trsm_rt, compute_trsm_ln_group_cost, eval_trsm_group, 1);
    registry.register_op("Potrf_L", true, analyze_potrf_l, compute_potrf_l_potrf_u_cost, eval_potrf_group, 0);
    registry.register_op("Potrf_U", true, analyze_potrf_u, compute_potrf_l_potrf_u_cost, eval_potrf_group, 0);
    registry.register_op("Orgqr", true, analyze_orgqr, compute_orgqr_cost, eval_orgqr);
    registry.register_op("Ormqr_LN", true, analyze_ormqr_ln, compute_ormqr_cost, eval_ormqr_group, 1);
    registry.register_op("Ormqr_LT", true, analyze_ormqr_lt, compute_ormqr_cost, eval_ormqr_group, 1);
    registry.register_op("Ormqr_RN", true, analyze_ormqr_rn, compute_ormqr_cost, eval_ormqr_group, 1);
    registry.register_op("Ormqr_RT", true, analyze_ormqr_rt, compute_ormqr_cost, eval_ormqr_group, 1);
    registry.register_op("Axpy", true, analyze_axpy, compute_axpy_cost, eval_axpy, 1);
}

} // namespace egraph
