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
    const DataBindings *data_bindings)
    : egraph(egraph), result(result) {

    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();

            if (auto data = get_matrix_data(egraph, class_id)) {
                if (data->has_symbolic_shape() && (!data_bindings || data_bindings->empty())) {
                    throw std::runtime_error("Cannot evaluate with symbolic matrices without data bindings.");
                }
                MatrixNode node_data;
                Shape shape = bind_shape(data->shape, size_bindings);
                node_data.rows = *std::get_if<int>(&shape.first);
                node_data.cols = *std::get_if<int>(&shape.second);
                if (std::holds_alternative<Op>(atom)) // op
                {
                    node_data.raw_data_vector = std::vector<double>(node_data.rows * node_data.cols);
                }
                if (std::holds_alternative<uint32_t>(atom)) // input matrices
                {
                    if (data->flags.is_identity || data->flags.is_zero)
                    // if Identity or Zero, fill different values
                    {
                        node_data.raw_data_vector = std::vector<double>(node_data.rows * node_data.cols);
                        for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                            if (data->flags.is_identity) {
                                node_data.raw_data_vector[i] =
                                    (i / node_data.rows == i % node_data.cols) ? 1.0 : 0.0; // Identity matrix
                            } else if (data->flags.is_zero) {
                                node_data.raw_data_vector[i] = 0.0; // Zero matrix
                            }
                        }
                    } else {
                        auto matrix_name = get_string_from_lookup(std::get<uint32_t>(atom));
                        if (data_bindings) {
                            if (data_bindings->find(matrix_name) == data_bindings->end()) {
                                throw std::runtime_error("Data binding for matrix " + matrix_name + " not provided.");
                            }
                            node_data.raw_data_vector = data_bindings->at(matrix_name);
                        } else {
                            node_data.raw_data_vector = std::vector<double>(node_data.rows * node_data.cols);
                            for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                                node_data.raw_data_vector[i] = static_cast<double>(rand()) / RAND_MAX;
                            }
                        }
                    }
                }
                data_storage[class_id] = node_data;
            } else if (std::holds_alternative<double>(atom)) {
                MatrixNode node_data;
                node_data.rows = 1;
                node_data.cols = 1;
                node_data.raw_data_vector = std::vector<double>(1);
                node_data.raw_data_vector[0] = std::get<double>(atom);
                data_storage[class_id] = node_data;
            } else if (auto data = get_tuple_data(egraph, class_id)) {
                TupleNode tuple_data;
                for (const auto &matrix_data : *data) {
                    MatrixNode node_data;
                    Shape shape = bind_shape(matrix_data.shape, size_bindings);
                    node_data.rows = *std::get_if<int>(&shape.first);
                    node_data.cols = *std::get_if<int>(&shape.second);
                    node_data.raw_data_vector = std::vector<double>(node_data.rows * node_data.cols);
                    tuple_data.matrices.push_back(node_data);
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
                        int index =
                            static_cast<int>(std::get<MatrixNode>(data_storage.at(index_id)).raw_data_vector[0]);
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
    return std::get<MatrixNode>(data_storage.at(result.execution_order.back())).raw_data_vector;
}

void Evaluator::dispatch_matrix_kernel(Op op, MatrixNode &output, const ENode *node) const {
    using enum Op;

    std::vector<MatrixNode> inputs;
    for (Id child_id : node->get_children()) {
        if (std::holds_alternative<MatrixNode>(data_storage.at(child_id))) {
            inputs.push_back(std::get<MatrixNode>(data_storage.at(child_id)));
        }
    }
    switch (op) {
    case Trsm_LN:
    case Trsm_LT:
    case Trsm_RN:
    case Trsm_RT: {
        std::copy(
            inputs[1].raw_data_vector.data(), inputs[1].raw_data_vector.data() + (output.rows * output.cols),
            output.raw_data_vector.data());
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo =
            std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? CblasLower : CblasUpper;

        CBLAS_SIDE side = (op == Op::Trsm_LN || op == Op::Trsm_LT) ? CblasLeft : CblasRight;
        CBLAS_TRANSPOSE trans = (op == Op::Trsm_LN || op == Op::Trsm_RN) ? CblasNoTrans : CblasTrans;

        cblas_dtrsm(
            CblasColMajor, side, uplo, trans, CblasNonUnit, output.rows, output.cols, 1.0,
            inputs[0].raw_data_vector.data(), inputs[0].rows, output.raw_data_vector.data(), output.rows);
        break;
    }
    case Gemv_N:
    case Gemv_T: {
        std::copy(
            inputs[2].raw_data_vector.data(), inputs[2].raw_data_vector.data() + output.rows,
            output.raw_data_vector.data());
        CBLAS_TRANSPOSE trans = (op == Op::Gemv_T) ? CblasTrans : CblasNoTrans;
        cblas_dgemv(
            CblasColMajor, trans, inputs[0].rows, inputs[0].cols, 1.0, inputs[0].raw_data_vector.data(), inputs[0].rows,
            inputs[1].raw_data_vector.data(), 1, 1.0, output.raw_data_vector.data(), 1);
        break;
    }
    case Syrk_N:
    case Syrk_T: {
        CBLAS_TRANSPOSE trans = (op == Op::Syrk_T) ? CblasTrans : CblasNoTrans;
        int k = (trans == CblasNoTrans) ? inputs[0].cols : inputs[0].rows;
        std::copy(
            inputs[1].raw_data_vector.data(), inputs[1].raw_data_vector.data() + (output.rows * output.cols),
            output.raw_data_vector.data());
        cblas_dsyrk(
            CblasColMajor, CblasUpper, trans, output.rows, k, 1.0, inputs[0].raw_data_vector.data(), inputs[0].rows,
            1.0, output.raw_data_vector.data(), output.rows);

        // Fill the lower triangular part of the matrix to make it symmetric (should be handled by storage format in the
        // future)
        for (int j = 0; j < output.cols; ++j) {
            for (int i = j + 1; i < output.rows; ++i) {
                output.raw_data_vector[i + j * output.rows] = output.raw_data_vector[j + i * output.rows];
            }
        }
        break;
    }
    case Trtri: {
        int n = output.rows;
        std::copy(
            inputs[0].raw_data_vector.data(), inputs[0].raw_data_vector.data() + n * n, output.raw_data_vector.data());
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        char uplo = std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? 'L' : 'U';
        LAPACKE_dtrtri(LAPACK_COL_MAJOR, uplo, 'N', n, output.raw_data_vector.data(), n);
        break;
    }

        // normally should be consumed as kernel parameters but explicit transpose might be needed sometimes
        // case Tr: {
        //     int rows = inputs[0].rows;
        //     int cols = inputs[0].cols;
        //     for (int i = 0; i < rows; ++i) {
        //         for (int j = 0; j < cols; ++j) {
        //             // Column major: input[i, j] = input.raw_data_vector[i + j * rows]
        //             // Column major: output[j, i] = output.raw_data_vector[j + i * cols]
        //             output.raw_data_vector[j + i * cols] = inputs[0].raw_data_vector[i + j * rows];
        //         }
        //     }
        //     break;
        // }

    case Orgqr: {
        Id geqrf_id = node->get_children()[0];
        const TupleNode &geqrf_tuple = std::get<TupleNode>(data_storage.at(geqrf_id));
        const MatrixNode &q_node = geqrf_tuple.matrices[0];
        int k = std::min(q_node.rows, q_node.cols);
        std::copy(q_node.raw_data_vector.begin(), q_node.raw_data_vector.end(), output.raw_data_vector.begin());
        LAPACKE_dorgqr(
            LAPACK_COL_MAJOR, output.rows, output.cols, k, output.raw_data_vector.data(), output.rows,
            geqrf_tuple.tau.data());
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
        // We reuse output.raw_data_vector as the temporary buffer by assigning c_node to it.
        int out_rows = output.rows;
        int out_cols = output.cols;
        output.raw_data_vector = c_node.raw_data_vector;

        LAPACKE_dormqr(
            LAPACK_COL_MAJOR, side, trans, m, n, k, a_node.raw_data_vector.data(), a_node.rows, geqrf_tuple.tau.data(),
            output.raw_data_vector.data(), m);

        // Extract the relevant part into output in-place
        if (out_rows != m) {
            for (int j = 0; j < out_cols; ++j) {
                for (int i = 0; i < out_rows; ++i) {
                    output.raw_data_vector[i + j * out_rows] = output.raw_data_vector[i + j * m];
                }
            }
            output.raw_data_vector.resize(out_rows * out_cols);
        }

        break;
    }
    case Gemm_NN:
    case Gemm_NT:
    case Gemm_TN:
    case Gemm_TT: {
        const MatrixNode &a_node = inputs[0];
        const MatrixNode &b_node = inputs[1];
        const MatrixNode &c_node = inputs[2];

        CBLAS_TRANSPOSE transA = (op == Op::Gemm_TN || op == Op::Gemm_TT) ? CblasTrans : CblasNoTrans;
        CBLAS_TRANSPOSE transB = (op == Op::Gemm_NT || op == Op::Gemm_TT) ? CblasTrans : CblasNoTrans;
        int k = (transA == CblasNoTrans) ? a_node.cols : a_node.rows;

        std::copy(c_node.raw_data_vector.begin(), c_node.raw_data_vector.end(), output.raw_data_vector.begin());
        cblas_dgemm(
            CblasColMajor, transA, transB, output.rows, output.cols, k, 1.0, a_node.raw_data_vector.data(), a_node.rows,
            b_node.raw_data_vector.data(), b_node.rows, 1.0, output.raw_data_vector.data(), output.rows);
        break;
    }
    default:
        throw std::runtime_error("Kernel not implemented for this operation: " + std::to_string(static_cast<int>(op)));
    }
}

void Evaluator::dispatch_factorization(Op op, const MatrixNode &input, TupleNode &output, const ENode *node) const {
    using enum Op;
    switch (op) {
    case Potrf_U: {
        MatrixNode &res = output.matrices[0];
        std::copy(
            input.raw_data_vector.data(), input.raw_data_vector.data() + (res.rows * res.cols),
            res.raw_data_vector.data());
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'U', res.rows, res.raw_data_vector.data(), res.rows);
        // Zero out the lower triangular part
        for (int j = 0; j < res.cols; ++j) {
            for (int i = j + 1; i < res.rows; ++i) {
                res.raw_data_vector[i + j * res.rows] = 0.0;
            }
        }
        break;
    }
    case Potrf_L: {
        MatrixNode &res = output.matrices[0];
        std::copy(
            input.raw_data_vector.data(), input.raw_data_vector.data() + (res.rows * res.cols),
            res.raw_data_vector.data());
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', res.rows, res.raw_data_vector.data(), res.rows);
        for (int j = 0; j < res.cols; ++j) {
            for (int i = 0; i < j; ++i) {
                res.raw_data_vector[i + j * res.rows] = 0.0;
            }
        }
        break;
    }
    case Geqrf: {
        std::vector<double> a_copy(input.raw_data_vector.size());
        std::copy(
            input.raw_data_vector.data(), input.raw_data_vector.data() + (input.rows * input.cols), a_copy.data());
        LAPACKE_dgeqrf(LAPACK_COL_MAJOR, input.rows, input.cols, a_copy.data(), input.rows, output.tau.data());

        // Fill the R matrix (upper triangular) in the output tuple, should be handled by storage format in the future
        if (output.matrices.size() > 1) {
            MatrixNode &r_node = output.matrices[1];
            for (int j = 0; j < r_node.cols; ++j) {
                for (int i = 0; i < r_node.rows; ++i) {
                    if (i > j) {
                        r_node.raw_data_vector[i + j * r_node.rows] = 0.0;
                    } else {
                        r_node.raw_data_vector[i + j * r_node.rows] = a_copy[i + j * input.rows];
                    }
                }
            }
        }
        if (!output.matrices.empty()) {
            MatrixNode &q_node = output.matrices[0];
            int k = std::min(input.rows, input.cols);
            // Copy the reflectors part of a_copy to q_node before generating Q or using it in Ormqr
            // upper triangular part of q_node will be overwritten anyway
            for (int j = 0; j < k; ++j) {
                for (int i = 0; i < q_node.rows; ++i) {
                    q_node.raw_data_vector[i + j * q_node.rows] = a_copy[i + j * input.rows];
                }
            }
        }
        break;
    }
    default:
        throw std::runtime_error(
            "Factorization not implemented for this operation: " + std::to_string(static_cast<int>(op)));
    }
}

void Evaluator::dispatch_get(const TupleNode &input_tuple, int index, MatrixNode &output) const {
    if (index < 0 || index >= static_cast<int>(input_tuple.matrices.size())) {
        throw std::runtime_error("Index out of bounds in Get evaluation: " + std::to_string(index));
    }
    const MatrixNode &source = input_tuple.matrices[index];
    std::copy(
        source.raw_data_vector.data(), source.raw_data_vector.data() + (output.rows * output.cols),
        output.raw_data_vector.data());
}
