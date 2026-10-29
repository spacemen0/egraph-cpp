#include "evaluator.h"
#include "basic_types.h"
#include "op_registry.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vecLib/cblas_new.h>
#include <vector>

#ifdef __APPLE__
#include "lapacke_accelerate.h"
#include <vecLib/cblas.h>
#else
#include <cblas.h>
#include <lapacke.h>
#endif

namespace egraph {

Evaluator::Evaluator(
    EGraph &egraph, const ExtractionResult &result, const SizeBindings &size_bindings,
    const DataBindings &data_bindings, const std::vector<Id> &preserved_ids)
    : egraph(egraph), result(result), data_bindings(data_bindings), preserved_ids(preserved_ids),
      size_bindings(size_bindings) {

    size_t N = this->result.execution_order.size();
    data_storage.resize(N);
    use_counts.resize(N, 0);

    Id max_id = 0;
    for (Id id : this->egraph.get_all_class_ids()) {
        max_id = std::max(max_id, id);
    }
    slot_map.assign(max_id + 1, -1);

    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = this->result.execution_order[slot];
        slot_map[class_id] = static_cast<int>(slot);
    }

    for (Id id : this->egraph.get_all_class_ids()) {
        Id root = this->egraph.find_class_id(id);
        if (root < slot_map.size() && slot_map[root] != -1) {
            slot_map[id] = slot_map[root];
        }
    }

    preserved_slots.assign(N, false);
    for (Id id : this->preserved_ids) {
        if (id < slot_map.size() && slot_map[id] != -1) {
            preserved_slots[slot_map[id]] = true;
        }
    }

    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = this->result.execution_order[slot];
        const ENode *node = this->result.choices.at(class_id);
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
                if (data->has_symbolic_shape() && (size_bindings.empty())) {
                    throw std::runtime_error("Cannot evaluate with symbolic matrices without size bindings.");
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
                    if (matrix_data.has_symbolic_shape() && (size_bindings.empty())) {
                        throw std::runtime_error("Cannot evaluate with symbolic matrices without size bindings.");
                    }
                    Shape shape = bind_shape(matrix_data.shape, size_bindings);
                    int r = *std::get_if<int>(&shape.first);
                    int c = *std::get_if<int>(&shape.second);
                    tuple_data.matrices.emplace_back(r, c);
                }
                if (const auto *op = std::get_if<Op>(&atom); op && (OpRegistry::name(*op) == "Geqrf")) {
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
            for (Id child_id : node->get_children()) {
                if (child_id < slot_map.size() && slot_map[child_id] != -1) {
                    use_counts[slot_map[child_id]]++;
                }
            }
        }
    }
}

// re-use the data pointer of a child node for output node if it is only used once and not marked as preserved
void Evaluator::setup_in_place_output(Id child_id, MatrixNode &output) {
    int child_slot = slot_map[child_id];
    auto &child_node = std::get<MatrixNode>(data_storage[child_slot]);
    int count = use_counts[child_slot];
    bool is_preserved = preserved_slots[child_slot];
    // the child is only used once, not marked as preserved, and by the time of calling this function, the child is
    // guaranteed to be evaluated already
    if (count == 1 && !is_preserved) {
        output.data_ptr = std::move(child_node.data_ptr);
        use_counts[child_slot] = 0;
    } else {
        output.data_ptr = std::make_shared<std::vector<double>>(*child_node.data_ptr);
        if (count > 0) {
            use_counts[child_slot]--;
        }
    }
    output.rows = child_node.rows;
    output.cols = child_node.cols;
    output.format = child_node.format;
}

void Evaluator::prepare_in_place(Id child_id, EvalValue &output) {
    if (std::holds_alternative<MatrixNode>(output)) {
        setup_in_place_output(child_id, std::get<MatrixNode>(output));
        std::get<MatrixNode>(output).ensure_general();
    } else if (std::holds_alternative<TupleNode>(output)) {
        auto &tuple = std::get<TupleNode>(output);
        if (!tuple.matrices.empty()) {

            // If introduce new factorizations, we will need mapping for which tuple value to write into
            setup_in_place_output(child_id, tuple.matrices[0]);
        }
    }
}

std::vector<double> Evaluator::evaluate() {
    size_t N = result.execution_order.size();
    for (size_t slot = 0; slot < N; ++slot) {
        Id class_id = result.execution_order[slot];
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();
            auto children = node->get_children();
            if (const auto *op = std::get_if<Op>(&atom)) {
                std::vector<const EvalValue *> inputs;
                inputs.reserve(children.size());
                for (Id child_id : children) {
                    if (child_id < slot_map.size() && slot_map[child_id] != -1) {
                        inputs.push_back(&data_storage[slot_map[child_id]]);
                    }
                }
                std::vector<Operand> operands;
                init_operands(egraph, children, operands, &size_bindings);

                const auto &desc = OpRegistry::instance().get(*op);
                if (desc.eval) {
                    if (desc.in_place_input.has_value()) {
                        int target_idx = *desc.in_place_input;
                        if (target_idx >= 0 && std::cmp_less(target_idx, children.size())) {
                            prepare_in_place(children[target_idx], data_storage[slot]);
                        }
                    }
                    desc.eval(desc.name, operands, inputs, data_storage[slot]);
                } else {
                    throw std::runtime_error("Kernel not implemented for this operation: " + desc.name);
                }
            }
        }
    }

    // root node should be a MatrixNode
    size_t root_slot = N - 1;
    const MatrixNode &res_node = std::get<MatrixNode>(data_storage[root_slot]);
    res_node.ensure_general();
    return res_node.vec();
}

std::vector<double> Evaluator::get_preserved(Id id) const {
    if (id < slot_map.size() && slot_map[id] != -1) {
        int slot = slot_map[id];
        if (std::holds_alternative<MatrixNode>(data_storage[slot])) {
            auto &node = const_cast<MatrixNode &>(std::get<MatrixNode>(data_storage[slot]));
            node.ensure_general();
            return node.vec();
        }
    }
    throw std::runtime_error("Preserved ID not found in evaluator storage.");
}

void eval_potrf_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output) {
    auto &tuple = std::get<TupleNode>(output);
    MatrixNode &res = tuple.matrices[0];
    char uplo = (op_name == "Potrf_U") ? 'U' : 'L';
    auto expected_format = (uplo == 'U') ? StorageFormat::SymmetricUpper : StorageFormat::SymmetricLower;
    if (res.format != expected_format)
        res.ensure_general();
    LAPACKE_dpotrf(LAPACK_COL_MAJOR, uplo, res.rows, res.data(), res.rows);
    res.format = (uplo == 'U') ? StorageFormat::TriangularUpper : StorageFormat::TriangularLower;
}

void eval_geqrf(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output) {
    auto &tuple = std::get<TupleNode>(output);
    MatrixNode &a_node = tuple.matrices[0];
    a_node.ensure_general();
    double *a_data = a_node.data();
    LAPACKE_dgeqrf(LAPACK_COL_MAJOR, a_node.rows, a_node.cols, a_data, a_node.rows, tuple.tau.data());

    if (tuple.matrices.size() > 1) {
        MatrixNode &r_node = tuple.matrices[1];
        double *r_data = r_node.data();
        for (int j = 0; j < r_node.cols; ++j) {
            for (int i = 0; i < r_node.rows; ++i) {
                if (i > j) {
                    r_data[i + j * r_node.rows] = 0.0;
                } else {
                    r_data[i + j * r_node.rows] = a_data[i + j * a_node.rows];
                }
            }
        }
    }
}

void eval_get(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output) {
    const auto &tuple = std::get<TupleNode>(*inputs[0]);
    int index = 0;
    if (const auto *idx_op = as_index(operands[1])) {
        index = *idx_op;
    } else {
        throw std::runtime_error("Invalid operands in Get Operation");
    }
    auto &out_mat = std::get<MatrixNode>(output);
    out_mat = tuple.matrices[index];
}

void eval_orgqr(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output) {
    const auto &tuple = std::get<TupleNode>(*inputs[0]);
    const MatrixNode &q_node = tuple.matrices[0];
    auto &out_mat = std::get<MatrixNode>(output);
    int k = std::min(q_node.rows, q_node.cols);
    out_mat = q_node;
    LAPACKE_dorgqr(LAPACK_COL_MAJOR, out_mat.rows, out_mat.cols, k, out_mat.data(), out_mat.rows, tuple.tau.data());
}

void eval_ormqr_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output) {
    const auto &tuple = std::get<TupleNode>(*inputs[0]);
    const auto &a_node = tuple.matrices[0];
    auto &out_mat = std::get<MatrixNode>(output);

    char side = (op_name == "Ormqr_LN" || op_name == "Ormqr_LT") ? 'L' : 'R';
    char trans = (op_name == "Ormqr_LN" || op_name == "Ormqr_RN") ? 'N' : 'T';
    int m = out_mat.rows;
    int n = out_mat.cols;
    int k = std::min(a_node.rows, a_node.cols);

    int out_rows = m;
    int out_cols = n;
    const auto *tuple_op = as_tuple(operands[0]);
    const auto *c_op = as_matrix(operands[1]);
    if (tuple_op && !tuple_op->empty() && c_op) {
        int q_rows = (*tuple_op)[0].rows;
        int q_cols = (*tuple_op)[0].cols;
        int c_r = c_op->rows;
        int c_c = c_op->cols;
        if (op_name == "Ormqr_LN") {
            out_rows = q_rows;
            out_cols = c_c;
        } else if (op_name == "Ormqr_LT") {
            out_rows = q_cols;
            out_cols = c_c;
        } else if (op_name == "Ormqr_RN") {
            out_rows = c_r;
            out_cols = q_cols;
        } else if (op_name == "Ormqr_RT") {
            out_rows = c_r;
            out_cols = q_rows;
        }
    }

    LAPACKE_dormqr(
        LAPACK_COL_MAJOR, side, trans, m, n, k, a_node.data(), a_node.rows, tuple.tau.data(), out_mat.data(), m);

    if (out_rows != m || out_cols != n) {
        double *out_data = out_mat.data();
        for (int j = 0; j < out_cols; ++j) {
            for (int i = 0; i < out_rows; ++i) {
                out_data[i + j * out_rows] = out_data[i + j * m];
            }
        }
        out_mat.vec().resize(static_cast<long>(out_rows) * out_cols);
        out_mat.rows = out_rows;
        out_mat.cols = out_cols;
    }
}

void eval_symm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    bool is_c_zero = as_matrix(operands[2])->is_zero();
    CBLAS_SIDE side = (op_name == "Symm_L") ? CblasLeft : CblasRight;
    CBLAS_UPLO uplo_cblas = CblasLower;
    if (inputs[0]->format == StorageFormat::SymmetricUpper || inputs[0]->format == StorageFormat::TriangularUpper) {
        uplo_cblas = CblasUpper;
    } else if (
        inputs[0]->format == StorageFormat::SymmetricLower || inputs[0]->format == StorageFormat::TriangularLower) {
        uplo_cblas = CblasLower;
    } else {
        uplo_cblas = as_matrix(operands[0])->is_upper_triangular() ? CblasUpper : CblasLower;
    }
    inputs[1]->ensure_general();
    double alpha = 1.0;
    double beta = is_c_zero ? 0.0 : 1.0;
    cblas_dsymm(
        CblasColMajor, side, uplo_cblas, output.rows, output.cols, alpha, inputs[0]->data(), inputs[0]->rows,
        inputs[1]->data(), inputs[1]->rows, beta, output.data(), output.rows);
}
void eval_trmm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    output.ensure_general();
    CBLAS_UPLO uplo = as_matrix(operands[0])->is_lower_triangular() ? CblasLower : CblasUpper;
    CBLAS_SIDE side = (op_name == "Trmm_LN" || op_name == "Trmm_LT") ? CblasLeft : CblasRight;
    CBLAS_TRANSPOSE trans = (op_name == "Trmm_LN" || op_name == "Trmm_RN") ? CblasNoTrans : CblasTrans;

    double alpha = 1.0;
    cblas_dtrmm(
        CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, alpha, inputs[0]->data(),
        inputs[0]->rows, output.data(), output.rows);
}

void eval_trsm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    CBLAS_UPLO uplo = as_matrix(operands[0])->is_lower_triangular() ? CblasLower : CblasUpper;
    CBLAS_SIDE side = (op_name == "Trsm_LN" || op_name == "Trsm_LT") ? CblasLeft : CblasRight;
    CBLAS_TRANSPOSE trans = (op_name == "Trsm_LN" || op_name == "Trsm_RN") ? CblasNoTrans : CblasTrans;

    double alpha = 1.0;
    cblas_dtrsm(
        CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, alpha, inputs[0]->data(),
        inputs[0]->rows, output.data(), output.rows);
}

void eval_gemm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    inputs[0]->ensure_general();
    inputs[1]->ensure_general();
    const MatrixNode &a_node = *inputs[0];
    const MatrixNode &b_node = *inputs[1];

    CBLAS_TRANSPOSE transA = (op_name == "Gemm_TN" || op_name == "Gemm_TT") ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE transB = (op_name == "Gemm_NT" || op_name == "Gemm_TT") ? CblasTrans : CblasNoTrans;
    int k = (transA == CblasNoTrans) ? a_node.cols : a_node.rows;

    bool is_c_zero = as_matrix(operands[2])->is_zero();
    double beta = is_c_zero ? 0.0 : 1.0;
    double alpha = 1.0;
    cblas_dgemm(
        CblasColMajor, transA, transB, output.rows, output.cols, k, alpha, a_node.data(), a_node.rows, b_node.data(),
        b_node.rows, beta, output.data(), output.rows);
}

void eval_gemv_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    inputs[0]->ensure_general();
    inputs[1]->ensure_general();
    CBLAS_TRANSPOSE trans = (op_name == "Gemv_T") ? CblasTrans : CblasNoTrans;
    bool is_c_zero = as_matrix(operands[2])->is_zero();
    double beta = is_c_zero ? 0.0 : 1.0;
    double alpha = 1.0;
    cblas_dgemv(
        CblasColMajor, trans, inputs[0]->rows, inputs[0]->cols, alpha, inputs[0]->data(), inputs[0]->rows,
        inputs[1]->data(), 1, beta, output.data(), 1);
}

void eval_syrk_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    inputs[0]->ensure_general();
    CBLAS_TRANSPOSE trans = (op_name == "Syrk_T") ? CblasTrans : CblasNoTrans;
    int k = (trans == CblasNoTrans) ? inputs[0]->cols : inputs[0]->rows;
    bool is_c_zero = as_matrix(operands[1])->is_zero();
    double beta = is_c_zero ? 0.0 : 1.0;

    CBLAS_UPLO uplo = CblasLower;
    double alpha = 1.0;
    cblas_dsyrk(
        CblasColMajor, uplo, trans, output.rows, k, alpha, inputs[0]->data(), inputs[0]->rows, beta, output.data(),
        output.rows);

    output.format = StorageFormat::SymmetricLower;
}

void eval_trtri(
    std::string_view, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &,
    MatrixNode &output) {
    int n = output.rows;
    char uplo = as_matrix(operands[0])->is_lower_triangular() ? 'L' : 'U';
    LAPACKE_dtrtri(LAPACK_COL_MAJOR, uplo, 'N', n, output.data(), n);
}

void eval_transpose(
    std::string_view, const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs, MatrixNode &output) {
    inputs[0]->ensure_general();
    int rows = inputs[0]->rows;
    int cols = inputs[0]->cols;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            output.data()[j + i * cols] = inputs[0]->data()[i + j * rows];
        }
    }
}

void eval_scale(
    std::string_view, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output) {
    double scalar_val = 1.0;
    if (inputs.size() > 1 && inputs[1]) {
        scalar_val = inputs[1]->data()[0];
    } else if (operands.size() > 1) {
        if (const auto *s = as_scalar(operands[1])) {
            if (!std::isnan(*s)) {
                scalar_val = *s;
            }
        } else if (const auto *i = as_index(operands[1])) {
            scalar_val = static_cast<double>(*i);
        }
    }
    cblas_dscal(output.rows * output.cols, scalar_val, output.data(), 1);
}

void eval_axpy(
    std::string_view, const std::vector<Operand> &, const std::vector<const MatrixNode *> &inputs, MatrixNode &output) {
    inputs[0]->ensure_general();
    cblas_daxpy(inputs[0]->rows * inputs[0]->cols, 1.0, inputs[0]->data(), 1, output.data(), 1);
}
} // namespace egraph
