#include "evaluator.h"
#include "basic_types.h"
#include "utils.h"
#include <cstdint>
#include <variant>
#include <vector>

#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <lapacke.h>
#include <vecLib/cblas.h>
#else
#include <cblas.h>
#include <lapacke.h>
#endif

Evaluator::Evaluator(
    EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings,
    const DataBindings &data_bindings)
    : egraph(egraph), result(result), data_bindings(data_bindings) {

    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();

            if (auto data = get_matrix_data(egraph, class_id)) {
                if (data->has_symbolic_shape() && (data_bindings.empty())) {
                    throw std::runtime_error("Cannot evaluate with symbolic matrices without data bindings.");
                }
                Shape shape = bind_shape(data->shape, size_bindings);
                int rows = *std::get_if<int>(&shape.first);
                int cols = *std::get_if<int>(&shape.second);
                MatrixNode node_data(rows, cols);

                if (std::holds_alternative<uint32_t>(atom)) // input matrices
                {

                    auto matrix_name = get_string_from_lookup(std::get<uint32_t>(atom));

                    if (data_bindings.find(matrix_name) == data_bindings.end()) {
                        if (data->flags.is_identity == false && data->flags.is_zero == false)
                            throw std::runtime_error("Data binding for matrix " + matrix_name + " not provided.");
                        else {
                            for (int i = 0; i < rows * cols; ++i) {
                                if (data->flags.is_identity) {
                                    node_data.vec()[i] = (i / rows == i % cols) ? 1.0 : 0.0;
                                } else if (data->flags.is_zero) {
                                    node_data.vec()[i] = 0.0;
                                }
                            }
                        }
                    } else {
                        node_data = MatrixNode(rows, cols, data_bindings.at(matrix_name));
                    }
                }
                data_storage[class_id] = node_data;
            } else if (const ScalarExpr *s = std::get_if<ScalarExpr>(&atom)) {
                MatrixNode node_data(1, 1);
                node_data.vec()[0] = s->evaluate(data_bindings);
                data_storage[class_id] = node_data;
            } else if (const int *i_val = std::get_if<int>(&atom)) {
                MatrixNode node_data(1, 1);
                node_data.vec()[0] = static_cast<double>(*i_val);
                data_storage[class_id] = node_data;
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
                data_storage[class_id] = tuple_data;
            }
        }
    }
    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            for (Id child_id : node->get_children()) {
                use_counts[child_id]++;
            }
        }
    }
}

void Evaluator::setup_in_place_output(Id child_id, MatrixNode &output) const {
    auto &child_node = std::get<MatrixNode>(data_storage.at(child_id));
    auto it = use_counts.find(child_id);
    if (it != use_counts.end() && it->second == 1) {
        output.data_ptr = std::move(child_node.data_ptr);
        it->second = 0;
    } else {
        output.data_ptr = std::make_shared<std::vector<double>>(*child_node.data_ptr);
        if (it != use_counts.end() && it->second > 0) {
            it->second--;
        }
    }
}

std::vector<double> Evaluator::evaluate() {
    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();
            if (const auto *op = std::get_if<Op>(&atom)) {
                auto it = data_storage.find(class_id);
                if (it != data_storage.end()) {
                    if (*op == Op::Get) {
                        MatrixNode &output = std::get<MatrixNode>(it->second);
                        Id tuple_id = node->get_children()[0];
                        Id index_id = node->get_children()[1];
                        const TupleNode &input_tuple = std::get<TupleNode>(data_storage.at(tuple_id));
                        int index = 0;
                        if (auto idx_opt = get_int_from_eclass(egraph, index_id); idx_opt.has_value()) {
                            index = idx_opt.value();
                        }
                        dispatch_get(input_tuple, index, output);
                    } else if (std::holds_alternative<TupleNode>(it->second)) {
                        TupleNode &output = std::get<TupleNode>(it->second);
                        Id input_id = node->get_children()[0];
                        const MatrixNode &input = std::get<MatrixNode>(data_storage.at(input_id));
                        dispatch_factorization(*op, input, output, node);
                    } else {
                        MatrixNode &output = std::get<MatrixNode>(it->second);
                        dispatch_matrix_kernel(*op, output, node);
                    }
                }
            }
        }
    }
    // root node should be a MatrixNode
    const MatrixNode &res_node = std::get<MatrixNode>(data_storage.at(result.execution_order.back()));
    return res_node.vec();
}

void Evaluator::dispatch_matrix_kernel(Op op, MatrixNode &output, const ENode *node) const {
    using enum Op;

    std::vector<const MatrixNode *> inputs;
    for (Id child_id : node->get_children()) {
        auto it = data_storage.find(child_id);
        if (it != data_storage.end() && std::holds_alternative<MatrixNode>(it->second)) {
            inputs.push_back(&std::get<MatrixNode>(it->second));
        }
    }
    switch (op) {
    case Trsm_LN:
    case Trsm_LT:
    case Trsm_RN:
    case Trsm_RT: {
        setup_in_place_output(node->get_children()[1], output);
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo =
            std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? CblasLower : CblasUpper;

        CBLAS_SIDE side = (op == Op::Trsm_LN || op == Op::Trsm_LT) ? CblasLeft : CblasRight;
        CBLAS_TRANSPOSE trans = (op == Op::Trsm_LN || op == Op::Trsm_RN) ? CblasNoTrans : CblasTrans;

        cblas_dtrsm(
            CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, 1.0, inputs[0]->data(),
            inputs[0]->rows, output.data(), output.rows);
        break;
    }
    case Gemv_N:
    case Gemv_T: {
        CBLAS_TRANSPOSE trans = (op == Op::Gemv_T) ? CblasTrans : CblasNoTrans;
        bool c_is_zero =
            std::get<MatrixProperty>(egraph.get_class_analysis_data(node->get_children()[1]).property).flags.is_zero;
        double beta = c_is_zero ? 0.0 : 1.0;
        if (!c_is_zero) {
            setup_in_place_output(node->get_children()[1], output);
        }
        cblas_dgemv(
            CblasColMajor, trans, inputs[0]->rows, inputs[0]->cols, 1.0, inputs[0]->data(), inputs[0]->rows,
            inputs[1]->data(), 1, beta, output.data(), 1);
        break;
    }
    case Syrk_N:
    case Syrk_T: {
        CBLAS_TRANSPOSE trans = (op == Op::Syrk_T) ? CblasTrans : CblasNoTrans;
        int k = (trans == CblasNoTrans) ? inputs[0]->cols : inputs[0]->rows;
        bool c_is_zero =
            std::get<MatrixProperty>(egraph.get_class_analysis_data(node->get_children()[1]).property).flags.is_zero;
        double beta = c_is_zero ? 0.0 : 1.0;
        if (!c_is_zero) {
            setup_in_place_output(node->get_children()[1], output);
        }
        cblas_dsyrk(
            CblasColMajor, CblasUpper, trans, output.rows, k, 1.0, inputs[0]->data(), inputs[0]->rows, beta,
            output.data(), output.rows);
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
        int rows = inputs[0]->rows;
        int cols = inputs[0]->cols;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                output.vec()[j + i * cols] = inputs[0]->data()[i + j * rows];
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
            }
        } else {
            throw std::runtime_error("Scale operation requires a scalar value as the second child.");
        }
        cblas_dscal(output.rows * output.cols, scalar_val, output.data(), 1);
        break;
    }

    case Orgqr: {
        Id geqrf_id = node->get_children()[0];
        const TupleNode &geqrf_tuple = std::get<TupleNode>(data_storage.at(geqrf_id));
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
        const TupleNode &geqrf_tuple = std::get<TupleNode>(data_storage.at(geqrf_id));
        const MatrixNode &c_node = std::get<MatrixNode>(data_storage.at(c_id));
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

        LAPACKE_dormqr(
            LAPACK_COL_MAJOR, side, trans, m, n, k, a_node.data(), a_node.rows, geqrf_tuple.tau.data(), output.data(),
            m);

        // Extract the relevant part into output in-place
        if (out_rows != m) {
            for (int j = 0; j < out_cols; ++j) {
                for (int i = 0; i < out_rows; ++i) {
                    output.vec()[i + j * out_rows] = output.vec()[i + j * m];
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
        }
        cblas_dgemm(
            CblasColMajor, transA, transB, output.rows, output.cols, k, 1.0, a_node.data(), a_node.rows, b_node.data(),
            b_node.rows, beta, output.data(), output.rows);
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
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'U', res.rows, res.data(), res.rows);
        break;
    }
    case Potrf_L: {
        MatrixNode &res = output.matrices[0];
        setup_in_place_output(node->get_children()[0], res);
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', res.rows, res.data(), res.rows);
        break;
    }
    case Geqrf: {
        setup_in_place_output(node->get_children()[0], output.matrices[0]);
        auto &a_data = output.matrices[0].vec();
        LAPACKE_dgeqrf(
            LAPACK_COL_MAJOR, input.rows, input.cols, output.matrices[0].data(), input.rows, output.tau.data());

        // Fill the R matrix (upper triangular) in the output tuple, should be handled by storage format in the
        // future
        if (output.matrices.size() > 1) {
            MatrixNode &r_node = output.matrices[1];
            for (int j = 0; j < r_node.cols; ++j) {
                for (int i = 0; i < r_node.rows; ++i) {
                    if (i > j) {
                        r_node.vec()[i + j * r_node.rows] = 0.0;
                    } else {
                        r_node.vec()[i + j * r_node.rows] = a_data[i + j * input.rows];
                    }
                }
            }
        }
        if (!output.matrices.empty()) {
            int k = std::min(input.rows, input.cols);
            // Copy the reflectors part of a_copy to q_node before generating Q or using it in Ormqr
            // upper triangular part of q_node will be overwritten anyway
            for (int j = 0; j < k; ++j) {
                for (int i = 0; i < output.matrices[0].rows; ++i) {
                    a_data[i + j * output.matrices[0].rows] = a_data[i + j * input.rows];
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
