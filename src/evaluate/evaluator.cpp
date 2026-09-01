#include "evaluator.h"
#include "basic_types.h"
#include "utils.h"
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#ifdef __APPLE__
#include "lapacke_accelerate.h"
#include <vecLib/cblas.h>
#else
#include <cblas.h>
#include <lapacke.h>
#endif

namespace egraph {

namespace {
// - Gemm/Symm/Syrk/Gemv: alpha and beta (C = alpha*op(A)*op(B) + beta*C)
// - Trsm/Trmm: alpha only
bool kernel_accepts_alpha(Op op) {
    using enum Op;
    switch (op) {
    case Gemm_NN:
    case Gemm_TN:
    case Gemm_NT:
    case Gemm_TT:
    case Symm_L:
    case Symm_R:
    case Syrk_N:
    case Syrk_T:
    case Gemv_N:
    case Gemv_T:
    case Trsm_LN:
    case Trsm_LT:
    case Trsm_RN:
    case Trsm_RT:
    case Trmm_LN:
    case Trmm_LT:
    case Trmm_RN:
    case Trmm_RT:
        return true;
    default:
        return false;
    }
}

// Index of the C (accumulator / output) child for kernels that have one, or -1.
int kernel_c_child_index(Op op) {
    using enum Op;
    switch (op) {
    case Gemm_NN:
    case Gemm_TN:
    case Gemm_NT:
    case Gemm_TT:
    case Symm_L:
    case Symm_R:
    case Trmm_LN:
    case Trmm_LT:
    case Trmm_RN:
    case Trmm_RT:
    case Gemv_N:
    case Gemv_T:
        return 2;
    case Syrk_N:
    case Syrk_T:
        return 1;
    default:
        return -1; // Trsm has no C child
    }
}

// Rebuilds execution_order (post-order DFS from the root class) after choices were rewritten.
std::vector<Id>
rebuild_execution_order(const EGraph &egraph, Id root_class, const std::unordered_map<Id, const ENode *> &choices) {
    std::vector<Id> order;
    std::unordered_set<Id> visited;
    std::function<void(Id)> dfs = [&](Id current_id) {
        Id current = egraph.find_class_id(current_id);
        if (visited.count(current)) {
            return;
        }
        visited.insert(current);
        auto it = choices.find(current);
        if (it != choices.end() && it->second) {
            for (Id child : it->second->get_children()) {
                dfs(child);
            }
        }
        order.push_back(current);
    };
    dfs(root_class);
    return order;
}
} // namespace

void fuse_scales_into_kernels(ExtractionResult &result, EGraph &egraph) {
    using enum Op;
    auto &choices = result.choices;

    // Count how many nodes reference each class, so we only fold scales into kernels whose
    // result is consumed exclusively by the Scale node (otherwise other consumers would be
    // wrongly scaled).
    std::unordered_map<Id, int> use_count;
    for (const auto &[cid, node] : choices) {
        if (!node) {
            continue;
        }
        for (Id child : node->get_children()) {
            use_count[egraph.find_class_id(child)]++;
        }
    }

    // 1) Scale(Axpy(C, K(...)), s) -> fused K with C as accumulator, alpha=beta=s.
    //    This eliminates the Axpy and the outer Scale entirely.
    // 2) Scale(K(...), s) -> fused K with trailing alpha (and beta=s when C is non-zero).
    std::vector<std::pair<Id, const ENode *>> scale_nodes;
    for (const auto &[cid, node] : choices) {
        if (!node) {
            continue;
        }
        const Atom &atom = node->get_atom();
        if (auto op = std::get_if<Op>(&atom); op && *op == Scale) {
            scale_nodes.push_back({cid, node});
        }
    }

    for (const auto &[scale_class, scale_node] : scale_nodes) {
        const auto &sch = scale_node->get_children();
        if (sch.size() != 2) {
            continue;
        }
        Id child_id = egraph.find_class_id(sch[0]);
        Id scalar_id = egraph.find_class_id(sch[1]); // canonicalize to class id

        // The scaled matrix must not be referenced anywhere else.
        if (use_count[child_id] != 1) {
            continue;
        }
        auto child_it = choices.find(child_id);
        if (child_it == choices.end() || !child_it->second) {
            continue;
        }
        const ENode *child_node = child_it->second;
        const Atom &child_atom = child_node->get_atom();
        auto child_op = std::get_if<Op>(&child_atom);
        if (!child_op) {
            continue; // scaling an input matrix, a transpose, etc.
        }

        auto make_fused = [&](Op op, Children children) -> const ENode * {
            auto node = std::make_shared<ENode>(children, Atom(op));
            result.owned_nodes.push_back(node);
            return node.get();
        };

        // ---- Pattern 1: Scale(Axpy(C, K), s) -> K(A, B, C, s, s) ----
        if (*child_op == Axpy && child_node->get_children().size() == 2) {
            Id acc_id = egraph.find_class_id(child_node->get_children()[0]); // C
            Id kernel_id = egraph.find_class_id(child_node->get_children()[1]);
            if (use_count[kernel_id] != 1) {
                continue;
            }
            auto kit = choices.find(kernel_id);
            if (kit == choices.end() || !kit->second) {
                continue;
            }
            const ENode *knode = kit->second;
            const Atom &katom = knode->get_atom();
            auto kop = std::get_if<Op>(&katom);
            if (!kop || !kernel_accepts_alpha(*kop)) {
                continue;
            }
            int c_idx = kernel_c_child_index(*kop);
            if (c_idx < 0 || static_cast<int>(knode->get_children().size()) <= c_idx) {
                continue;
            }
            const auto &kch = knode->get_children();
            // K's own C must be Zero for this rewrite to be valid.
            Id k_c = egraph.find_class_id(kch[c_idx]);
            auto c_prop = egraph.get_class_analysis_data(k_c);
            bool k_c_zero = std::get<MatrixProperty>(c_prop.property).flags.is_zero;
            if (!k_c_zero) {
                continue;
            }
            Children fused_children = kch;
            fused_children[c_idx] = acc_id; // accumulate into C
            if (*kop == Trsm_LN || *kop == Trsm_LT || *kop == Trsm_RN || *kop == Trsm_RT) {
                fused_children.push_back(scalar_id); // alpha only
            } else {
                fused_children.push_back(scalar_id); // alpha
                fused_children.push_back(scalar_id); // beta
            }
            choices[scale_class] = make_fused(*kop, std::move(fused_children));
            choices.erase(child_id);
            choices.erase(kernel_id);
            continue;
        }

        // ---- Pattern 2: Scale(K(...), s) -> fused K with trailing alpha/beta ----
        if (!kernel_accepts_alpha(*child_op)) {
            continue;
        }
        const auto &kch = child_node->get_children();
        int c_idx = kernel_c_child_index(*child_op);
        Children fused_children = kch;
        bool has_nonzero_c = false;
        if (c_idx >= 0 && static_cast<int>(kch.size()) > c_idx) {
            Id k_c = egraph.find_class_id(kch[c_idx]);
            auto c_prop = egraph.get_class_analysis_data(k_c);
            has_nonzero_c = !std::get<MatrixProperty>(c_prop.property).flags.is_zero;
        }
        fused_children.push_back(scalar_id); // alpha
        // Trsm/Trmm have no beta; Gemm/Symm/Syrk/Gemv accumulate with beta=s into non-zero C.
        if (c_idx >= 0 && has_nonzero_c) {
            fused_children.push_back(scalar_id); // beta
        }
        choices[scale_class] = make_fused(*child_op, std::move(fused_children));
        choices.erase(child_id);
    }

    // Rebuild the execution order from the (possibly unchanged) root class, dropping any
    // classes that were fused away.
    if (!result.execution_order.empty()) {
        Id root = egraph.find_class_id(result.execution_order.back());
        result.execution_order = rebuild_execution_order(egraph, root, choices);
    }
}

Evaluator::Evaluator(
    EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings,
    const DataBindings &data_bindings)
    : egraph(egraph), result(result), data_bindings(data_bindings) {

    size_t N = result.execution_order.size();
    data_storage.resize(N);
    use_counts.resize(N, 0);

    Id max_id = 0;
    for (Id class_id : result.execution_order) {
        max_id = std::max(max_id, class_id);
    }
    slot_map.assign(max_id + 1, -1);

    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = result.execution_order[slot];
        slot_map[class_id] = static_cast<int>(slot);
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();

            // Scalars and integer constants are stored as 1x1 matrices. Check the atom first
            if (const ScalarExpr *s = std::get_if<ScalarExpr>(&atom)) {
                MatrixNode node_data(1, 1);
                node_data.data()[0] = s->evaluate(data_bindings);
                data_storage[slot] = node_data;
            } else if (const int *i_val = std::get_if<int>(&atom)) {
                MatrixNode node_data(1, 1);
                node_data.data()[0] = static_cast<double>(*i_val);
                data_storage[slot] = node_data;
            } else if (auto data = get_matrix_data(egraph, class_id)) {
                if (data->has_symbolic_shape() && (data_bindings.empty())) {
                    throw std::runtime_error("Cannot evaluate with symbolic matrices without data bindings.");
                }
                Shape shape = bind_shape(data->shape, size_bindings);
                int rows = *std::get_if<int>(&shape.first);
                int cols = *std::get_if<int>(&shape.second);

                if (std::holds_alternative<uint32_t>(atom)) // input matrices
                {

                    auto matrix_name = get_string_from_lookup(std::get<uint32_t>(atom));

                    if (data_bindings.find(matrix_name) == data_bindings.end()) {
                        if (data->flags.is_identity == false && data->flags.is_zero == false)
                            throw std::runtime_error("Data binding for matrix " + matrix_name + " not provided.");
                        else {
                            MatrixNode node_data(rows, cols);
                            for (int i = 0; i < rows * cols; ++i) {
                                if (data->flags.is_identity) {
                                    node_data.data()[i] = (i / rows == i % cols) ? 1.0 : 0.0;
                                } else if (data->flags.is_zero) {
                                    node_data.data()[i] = 0.0;
                                }
                            }
                            data_storage[slot] = node_data;
                        }
                    } else {
                        MatrixNode node_data(rows, cols, data_bindings.at(matrix_name));
                        data_storage[slot] = node_data;
                    }
                } else { // Ops, Intermediate matrices, initialize with default
                    MatrixNode node_data(rows, cols);
                    data_storage[slot] = node_data;
                }

            } else if (auto data = get_tuple_data(egraph, class_id)) {
                TupleNode tuple_data;
                for (const auto &matrix_data : *data) {
                    Shape shape = bind_shape(matrix_data.shape, size_bindings);
                    int r = *std::get_if<int>(&shape.first);
                    int c = *std::get_if<int>(&shape.second);
                    tuple_data.matrices.emplace_back(r, c);
                }
                if (const auto *op = std::get_if<Op>(&atom); op && (*op == Op::Geqrf)) {
                    if (!tuple_data.matrices.empty()) {
                        int min_dim = std::min(tuple_data.matrices[0].rows, tuple_data.matrices[0].cols);
                        tuple_data.tau = std::vector<double>(min_dim);
                    }
                }
                data_storage[slot] = tuple_data;
            }
        }
    }
    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = result.execution_order[slot];
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();
            if (const auto *op = std::get_if<Op>(&atom)) {
                if (*op == Op::Potrf_L) {
                    prefer_upper_triangular[node->get_children()[0]] = false;
                } else if (*op == Op::Potrf_U) {
                    prefer_upper_triangular[node->get_children()[0]] = true;
                }
            }
            for (Id child_id : node->get_children()) {
                if (child_id < slot_map.size() && slot_map[child_id] != -1) {
                    use_counts[slot_map[child_id]]++;
                }
            }
        }
    }
}

void Evaluator::setup_in_place_output(Id child_id, MatrixNode &output) const {
    int child_slot = slot_map[child_id];
    auto &child_node = std::get<MatrixNode>(data_storage[child_slot]);
    int count = use_counts[child_slot];
    // the child is only used once, and by the time of calling this function, the child is guaranteed to be evaluated
    // already
    if (count == 1) {
        output.data_ptr = std::move(child_node.data_ptr);
        use_counts[child_slot] = 0;
    } else {
        output.data_ptr = std::make_shared<std::vector<double>>(*child_node.data_ptr);
        if (count > 0) {
            use_counts[child_slot]--;
        }
    }
    output.format = child_node.format;
}

std::vector<double> Evaluator::evaluate() {
    size_t N = result.execution_order.size();
    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = result.execution_order[slot];
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();
            if (const auto *op = std::get_if<Op>(&atom)) {
                auto &storage = data_storage[slot];
                if (*op == Op::Get) {
                    MatrixNode &output = std::get<MatrixNode>(storage);
                    Id tuple_id = node->get_children()[0];
                    Id index_id = node->get_children()[1];
                    const TupleNode &input_tuple = std::get<TupleNode>(data_storage[slot_map[tuple_id]]);
                    int index = 0;
                    if (auto idx_opt = get_int_from_eclass(egraph, index_id); idx_opt.has_value()) {
                        index = idx_opt.value();
                    }
                    dispatch_get(input_tuple, index, output);
                } else if (std::holds_alternative<TupleNode>(storage)) {
                    TupleNode &output = std::get<TupleNode>(storage);
                    Id input_id = node->get_children()[0];
                    const MatrixNode &input = std::get<MatrixNode>(data_storage[slot_map[input_id]]);
                    dispatch_factorization(*op, input, output, node);
                } else {
                    MatrixNode &output = std::get<MatrixNode>(storage);
                    dispatch_matrix_kernel(*op, output, node, class_id);
                }
            }
        }
    }
    // root node should be a MatrixNode
    size_t root_slot = N - 1;
    const MatrixNode &res_node = std::get<MatrixNode>(data_storage[root_slot]);
    return res_node.vec();
}

void Evaluator::dispatch_matrix_kernel(Op op, MatrixNode &output, const ENode *node, Id class_id) const {
    using enum Op;

    std::vector<const MatrixNode *> inputs;
    for (Id child_id : node->get_children()) {
        if (child_id < slot_map.size() && slot_map[child_id] != -1) {
            int slot = slot_map[child_id];
            if (std::holds_alternative<MatrixNode>(data_storage[slot])) {
                inputs.push_back(&std::get<MatrixNode>(data_storage[slot]));
            }
        }
    }
    switch (op) {
    case Symm_L:
    case Symm_R: {
        auto c_prop = egraph.get_class_analysis_data(node->get_children()[2]);
        bool is_c_zero;
        if (auto p = std::get_if<MatrixProperty>(&c_prop.property)) {
            is_c_zero = p->flags.is_zero;
        }
        if (!is_c_zero) {
            setup_in_place_output(node->get_children()[2], output);
            output.ensure_general();
        }
        CBLAS_SIDE side = (op == Op::Symm_L) ? CblasLeft : CblasRight;
        auto a_prop = egraph.get_class_analysis_data(node->get_children()[0]);
        char uplo = std::get<MatrixProperty>(a_prop.property).flags.is_lower_triangular ? 'L' : 'U';
        // Fused alpha/beta from trailing scalar children (fuse_scales_into_kernels).
        double alpha = 1.0;
        double beta = is_c_zero ? 0.0 : 1.0;
        const auto &symm_ch = node->get_children();
        if (symm_ch.size() > 3 && inputs.size() > 3) {
            alpha = inputs[3]->data()[0];
        }
        if (symm_ch.size() > 4 && inputs.size() > 4) {
            beta = inputs[4]->data()[0];
        }
        cblas_dsymm(
            CblasColMajor, side, uplo == 'L' ? CblasLower : CblasUpper, output.rows, output.cols, alpha,
            inputs[0]->data(), inputs[0]->rows, inputs[1]->data(), inputs[1]->rows, beta, output.data(), output.rows);
        break;
    }
    case Trmm_LN:
    case Trmm_LT:
    case Trmm_RN:
    case Trmm_RT: {
        auto c_prop = egraph.get_class_analysis_data(node->get_children()[2]);
        bool is_c_zero = false;
        if (auto p = std::get_if<MatrixProperty>(&c_prop.property)) {
            is_c_zero = p->flags.is_zero;
        }

        auto a_prop = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo = std::get<MatrixProperty>(a_prop.property).flags.is_lower_triangular ? CblasLower : CblasUpper;
        CBLAS_SIDE side = (op == Op::Trmm_LN || op == Op::Trmm_LT) ? CblasLeft : CblasRight;
        CBLAS_TRANSPOSE trans = (op == Op::Trmm_LN || op == Op::Trmm_RN) ? CblasNoTrans : CblasTrans;

        if (is_c_zero) {
            setup_in_place_output(node->get_children()[1], output);
            output.ensure_general();
            // Fused alpha from trailing scalar child (fuse_scales_into_kernels).
            double alpha = 1.0;
            const auto &trmm_ch = node->get_children();
            if (trmm_ch.size() > 3 && inputs.size() > 3) {
                alpha = inputs[3]->data()[0];
            }
            cblas_dtrmm(
                CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, alpha, inputs[0]->data(),
                inputs[0]->rows, output.data(), output.rows);
        } else {
            setup_in_place_output(node->get_children()[2], output);
            output.ensure_general();
            MatrixNode tempB;
            setup_in_place_output(node->get_children()[1], tempB);
            tempB.ensure_general();
            cblas_dtrmm(
                CblasColMajor, side, uplo, trans, CblasNonUnit, tempB.rows, tempB.cols, 1.0, inputs[0]->data(),
                inputs[0]->rows, tempB.data(), tempB.rows);
            cblas_daxpy(output.rows * output.cols, 1.0, tempB.data(), 1, output.data(), 1);
        }
        break;
    }
    case Trsm_LN:
    case Trsm_LT:
    case Trsm_RN:
    case Trsm_RT: {
        setup_in_place_output(node->get_children()[1], output);
        output.ensure_general();
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo =
            std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? CblasLower : CblasUpper;

        CBLAS_SIDE side = (op == Op::Trsm_LN || op == Op::Trsm_LT) ? CblasLeft : CblasRight;
        CBLAS_TRANSPOSE trans = (op == Op::Trsm_LN || op == Op::Trsm_RN) ? CblasNoTrans : CblasTrans;

        // Fused alpha from trailing scalar child (fuse_scales_into_kernels).
        double alpha = 1.0;
        const auto &trsm_ch = node->get_children();
        if (trsm_ch.size() > 2 && inputs.size() > 2) {
            alpha = inputs[2]->data()[0];
        }
        cblas_dtrsm(
            CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, alpha, inputs[0]->data(),
            inputs[0]->rows, output.data(), output.rows);
        break;
    }
    case Gemv_N:
    case Gemv_T: {
        inputs[0]->ensure_general();
        inputs[1]->ensure_general();
        CBLAS_TRANSPOSE trans = (op == Op::Gemv_T) ? CblasTrans : CblasNoTrans;
        bool c_is_zero =
            std::get<MatrixProperty>(egraph.get_class_analysis_data(node->get_children()[2]).property).flags.is_zero;
        double beta = c_is_zero ? 0.0 : 1.0;
        if (!c_is_zero) {
            setup_in_place_output(node->get_children()[2], output);
            output.ensure_general();
        }
        cblas_dgemv(
            CblasColMajor, trans, inputs[0]->rows, inputs[0]->cols, 1.0, inputs[0]->data(), inputs[0]->rows,
            inputs[1]->data(), 1, beta, output.data(), 1);
        break;
    }
    case Syrk_N:
    case Syrk_T: {
        inputs[0]->ensure_general();
        CBLAS_TRANSPOSE trans = (op == Op::Syrk_T) ? CblasTrans : CblasNoTrans;
        int k = (trans == CblasNoTrans) ? inputs[0]->cols : inputs[0]->rows;
        bool c_is_zero =
            std::get<MatrixProperty>(egraph.get_class_analysis_data(node->get_children()[1]).property).flags.is_zero;
        double beta = c_is_zero ? 0.0 : 1.0;
        if (!c_is_zero) {
            setup_in_place_output(node->get_children()[1], output);
        }

        CBLAS_UPLO uplo = CblasLower;
        if (prefer_upper_triangular.count(class_id)) {
            uplo = prefer_upper_triangular.at(class_id) ? CblasUpper : CblasLower;
        }

        // Fused alpha/beta from trailing scalar children (fuse_scales_into_kernels).
        double alpha = 1.0;
        const auto &syrk_ch = node->get_children();
        if (syrk_ch.size() > 2 && inputs.size() > 2) {
            alpha = inputs[2]->data()[0];
        }
        if (syrk_ch.size() > 3 && inputs.size() > 3) {
            beta = inputs[3]->data()[0];
        }
        cblas_dsyrk(
            CblasColMajor, uplo, trans, output.rows, k, alpha, inputs[0]->data(), inputs[0]->rows, beta, output.data(),
            output.rows);

        output.format = (uplo == CblasUpper) ? StorageFormat::SymmetricUpper : StorageFormat::SymmetricLower;
        break;
    }
    case Trtri: {
        int n = output.rows;
        setup_in_place_output(node->get_children()[0], output);
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        char uplo = std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? 'L' : 'U';
        LAPACKE_dtrtri(LAPACK_COL_MAJOR, uplo, 'N', n, output.data(), n);
        break;
    }

        // normally should be consumed as kernel parameters but explicit transpose might be needed sometimes
    case Tr: {
        inputs[0]->ensure_general();
        int rows = inputs[0]->rows;
        int cols = inputs[0]->cols;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                output.data()[j + i * cols] = inputs[0]->data()[i + j * rows];
            }
        }
        break;
    }

    case Scale: {
        setup_in_place_output(node->get_children()[0], output);
        double scalar_val = 1.0;
        Id scalar_id = node->get_children()[1];
        auto choice_it = result.choices.find(scalar_id);
        if (choice_it != result.choices.end() && choice_it->second) {
            const Atom &atom = choice_it->second->get_atom();
            if (const ScalarExpr *s = std::get_if<ScalarExpr>(&atom)) {
                scalar_val = s->evaluate(data_bindings);
            } else {
                throw std::runtime_error("Scale operation requires a scalar value as the second child.");
            }
        } else {
            throw std::runtime_error("Scale operation requires a scalar value as the second child.");
        }
        cblas_dscal(output.rows * output.cols, scalar_val, output.data(), 1);
        break;
    }

    case Orgqr: {
        Id geqrf_id = node->get_children()[0];
        const TupleNode &geqrf_tuple = std::get<TupleNode>(data_storage[slot_map[geqrf_id]]);
        const MatrixNode &q_node = geqrf_tuple.matrices[0];
        int k = std::min(q_node.rows, q_node.cols);
        output = q_node; // always overwrite q_node with the result of orgqr (which is Q)
        LAPACKE_dorgqr(
            LAPACK_COL_MAJOR, output.rows, output.cols, k, output.data(), output.rows, geqrf_tuple.tau.data());
        break;
    }
    case Ormqr_LN:
    case Ormqr_LT:
    case Ormqr_RN:
    case Ormqr_RT: {
        Id geqrf_id = node->get_children()[0];
        Id c_id = node->get_children()[1];
        const TupleNode &geqrf_tuple = std::get<TupleNode>(data_storage[slot_map[geqrf_id]]);
        const MatrixNode &c_node = std::get<MatrixNode>(data_storage[slot_map[c_id]]);
        const MatrixNode &a_node = geqrf_tuple.matrices[0]; // householder

        char side = (op == Ormqr_LN || op == Ormqr_LT) ? 'L' : 'R';
        char trans = (op == Ormqr_LN || op == Ormqr_RN) ? 'N' : 'T';
        int m = c_node.rows;
        int n = c_node.cols;
        int k = std::min(a_node.rows, a_node.cols);

        // dormqr requires C to be m x n and overwrites it.
        int out_rows = output.rows;
        int out_cols = output.cols;
        setup_in_place_output(node->get_children()[1], output);
        output.ensure_general();

        LAPACKE_dormqr(
            LAPACK_COL_MAJOR, side, trans, m, n, k, a_node.data(), a_node.rows, geqrf_tuple.tau.data(), output.data(),
            m);

        // Extract the relevant part into output in-place
        if (out_rows != m) {
            double *out_data = output.data();
            for (int j = 0; j < out_cols; ++j) {
                for (int i = 0; i < out_rows; ++i) {
                    out_data[i + j * out_rows] = out_data[i + j * m];
                }
            }
            output.vec().resize(out_rows * out_cols);
        }

        break;
    }
    case Gemm_NN:
    case Gemm_NT:
    case Gemm_TN:
    case Gemm_TT: {
        inputs[0]->ensure_general();
        inputs[1]->ensure_general();
        const MatrixNode &a_node = *inputs[0];
        const MatrixNode &b_node = *inputs[1];

        CBLAS_TRANSPOSE transA = (op == Op::Gemm_TN || op == Op::Gemm_TT) ? CblasTrans : CblasNoTrans;
        CBLAS_TRANSPOSE transB = (op == Op::Gemm_NT || op == Op::Gemm_TT) ? CblasTrans : CblasNoTrans;
        int k = (transA == CblasNoTrans) ? a_node.cols : a_node.rows;

        // If c_node is Zero, beta = 0.0 and no copy is needed.
        bool c_is_zero =
            std::get<MatrixProperty>(egraph.get_class_analysis_data(node->get_children()[2]).property).flags.is_zero;
        double beta = c_is_zero ? 0.0 : 1.0;
        if (!c_is_zero) {
            setup_in_place_output(node->get_children()[2], output);
            output.ensure_general();
        }
        // Fused alpha/beta from trailing scalar children (fuse_scales_into_kernels).
        double alpha = 1.0;
        const auto &gemm_ch = node->get_children();
        if (gemm_ch.size() > 3 && inputs.size() > 3 && inputs[3] && inputs[3]->data()) {
            alpha = inputs[3]->data()[0];
        }
        if (gemm_ch.size() > 4 && inputs.size() > 4 && inputs[4] && inputs[4]->data()) {
            beta = inputs[4]->data()[0];
        }
        cblas_dgemm(
            CblasColMajor, transA, transB, output.rows, output.cols, k, alpha, a_node.data(), a_node.rows,
            b_node.data(), b_node.rows, beta, output.data(), output.rows);
        break;
    }
    case Axpy: {
        inputs[0]->ensure_general();
        setup_in_place_output(node->get_children()[1], output);
        output.ensure_general();
        cblas_daxpy(inputs[0]->rows * inputs[0]->cols, 1.0, inputs[0]->data(), 1, output.data(), 1);
        break;
    }
    default:
        throw std::runtime_error("Kernel not implemented for this operation: " + atom_to_string(op));
    }
}

void Evaluator::dispatch_factorization(Op op, const MatrixNode &input, TupleNode &output, const ENode *node) const {
    using enum Op;
    switch (op) {
    case Potrf_U: {
        MatrixNode &res = output.matrices[0];
        setup_in_place_output(node->get_children()[0], res);
        if (res.format != StorageFormat::SymmetricUpper)
            res.ensure_general();
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'U', res.rows, res.data(), res.rows);
        res.format = StorageFormat::TriangularUpper;
        break;
    }
    case Potrf_L: {
        MatrixNode &res = output.matrices[0];
        setup_in_place_output(node->get_children()[0], res);
        if (res.format != StorageFormat::SymmetricLower)
            res.ensure_general();
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', res.rows, res.data(), res.rows);
        res.format = StorageFormat::TriangularLower;
        break;
    }
    case Geqrf: {
        setup_in_place_output(node->get_children()[0], output.matrices[0]);
        output.matrices[0].ensure_general();
        double *a_data = output.matrices[0].data();
        LAPACKE_dgeqrf(
            LAPACK_COL_MAJOR, input.rows, input.cols, output.matrices[0].data(), input.rows, output.tau.data());

        // Fill the R matrix (upper triangular) in the output tuple, should be handled by storage format in the
        // future
        if (output.matrices.size() > 1) {
            MatrixNode &r_node = output.matrices[1];
            double *r_data = r_node.data();
            for (int j = 0; j < r_node.cols; ++j) {
                for (int i = 0; i < r_node.rows; ++i) {
                    if (i > j) {
                        r_data[i + j * r_node.rows] = 0.0;
                    } else {
                        r_data[i + j * r_node.rows] = a_data[i + j * input.rows];
                    }
                }
            }
        }

        break;
    }
    default:
        throw std::runtime_error("Factorization not implemented for this operation: " + atom_to_string(op));
    }
}

void Evaluator::dispatch_get(const TupleNode &input_tuple, int index, MatrixNode &output) const {
    if (index < 0 || index >= static_cast<int>(input_tuple.matrices.size())) {
        throw std::runtime_error("Index out of bounds in Get evaluation: " + std::to_string(index));
    }
    const MatrixNode &source = input_tuple.matrices[index];
    output = source;
}

} // namespace egraph
