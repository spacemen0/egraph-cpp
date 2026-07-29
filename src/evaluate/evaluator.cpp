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

Evaluator::Evaluator(EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings)
    : egraph(egraph), result(result) {

    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();

            if (auto data = get_matrix_data(egraph, class_id)) {
                if (data->has_symbolic_shape() && (!size_bindings || size_bindings->empty())) {
                    throw std::runtime_error("Cannot evaluate with symbolic shapes without size bindings.");
                }
                MatrixNode node_data;
                Shape shape = bind_shape(data->shape, size_bindings);
                node_data.rows = *std::get_if<int>(&shape.first);
                node_data.cols = *std::get_if<int>(&shape.second);
                if (std::holds_alternative<Op>(atom)) // op
                {
                    node_data.data = new double[node_data.rows * node_data.cols];
                }
                if (std::holds_alternative<uint32_t>(atom)) // matrix
                {
                    // if Identity or Zero, fill different values
                    node_data.data = new double[node_data.rows * node_data.cols];
                    for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                        if (data->flags.is_identity) {
                            node_data.data[i] =
                                (i / node_data.rows == i % node_data.cols) ? 1.0 : 0.0; // Identity matrix
                        } else if (data->flags.is_zero) {
                            node_data.data[i] = 0.0; // Zero matrix
                        } else {
                            node_data.data[i] = static_cast<double>(rand()) / RAND_MAX; // Random values
                        }
                    }
                }
                data_storage[class_id] = node_data;
            } else if (std::holds_alternative<double>(atom)) {
                MatrixNode node_data;
                node_data.rows = 1;
                node_data.cols = 1;
                node_data.data = new double{std::get<double>(atom)};
                data_storage[class_id] = node_data;
            } else if (auto data = get_tuple_data(egraph, class_id)) {
                TupleNode tuple_data;
                for (const auto &matrix_data : *data) {
                    MatrixNode node_data;
                    Shape shape = bind_shape(matrix_data.shape, size_bindings);
                    node_data.rows = *std::get_if<int>(&shape.first);
                    node_data.cols = *std::get_if<int>(&shape.second);
                    node_data.data = new double[node_data.rows * node_data.cols];
                    for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                        node_data.data[i] = static_cast<double>(rand()) / RAND_MAX; // Random values
                    }
                    tuple_data.matrices.push_back(node_data);
                }
                if (const auto *op = std::get_if<Op>(&atom); op && (*op == Op::Geqrf)) {
                    if (!tuple_data.matrices.empty()) {
                        int min_dim = std::min(tuple_data.matrices[0].rows, tuple_data.matrices[0].cols);
                        tuple_data.tau = new double[min_dim];
                    }
                }
                data_storage[class_id] = tuple_data;
            }
        }
    }
}

double *Evaluator::evaluate() {
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
                        int index = static_cast<int>(*std::get<MatrixNode>(data_storage.at(index_id)).data);
                        dispatch_get(input_tuple, index, output);
                    } else if (std::holds_alternative<TupleNode>(it->second)) {
                        TupleNode &output = std::get<TupleNode>(it->second);
                        Id input_id = node->get_children()[0];
                        const MatrixNode &input = std::get<MatrixNode>(data_storage.at(input_id));
                        dispatch_factorization(*op, input, output, node);
                    } else {
                        MatrixNode &output = std::get<MatrixNode>(it->second);
                        std::vector<MatrixNode> inputs;
                        for (Id child_id : node->get_children()) {
                            inputs.push_back(std::get<MatrixNode>(data_storage.at(child_id)));
                        }
                        dispatch_matrix_kernel(*op, inputs, output, node);
                    }
                }
            }
        }
    }
    // root node should be a MatrixNode
    return std::get<MatrixNode>(data_storage.at(result.execution_order.back())).data;
}

void Evaluator::dispatch_matrix_kernel(
    Op op, const std::vector<MatrixNode> &inputs, MatrixNode &output, const ENode *node) const {
    using enum Op;
    switch (op) {
    case Trsm_LN: {
        std::copy(inputs[1].data, inputs[1].data + (output.rows * output.cols), output.data);
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo =
            std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? CblasLower : CblasUpper;
        cblas_dtrsm(
            CblasColMajor, CblasLeft, uplo, CblasNoTrans, CblasNonUnit, output.rows, output.cols, 1.0, inputs[0].data,
            inputs[0].rows, output.data, output.rows);
        break;
    }
    case Trsm_LT: {
        std::copy(inputs[1].data, inputs[1].data + (output.rows * output.cols), output.data);
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        CBLAS_UPLO uplo =
            std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? CblasLower : CblasUpper;
        cblas_dtrsm(
            CblasColMajor, CblasLeft, uplo, CblasTrans, CblasNonUnit, output.rows, output.cols, 1.0, inputs[0].data,
            inputs[0].rows, output.data, output.rows);
        break;
    }
    case Gemv_N: {
        std::copy(inputs[2].data, inputs[2].data + output.rows, output.data);
        cblas_dgemv(
            CblasColMajor, CblasNoTrans, inputs[0].rows, inputs[0].cols, 1.0, inputs[0].data, inputs[0].rows,
            inputs[1].data, 1, 1.0, output.data, 1);
        break;
    }
    case Gemv_T: {
        std::copy(inputs[2].data, inputs[2].data + output.rows, output.data);
        cblas_dgemv(
            CblasColMajor, CblasTrans, inputs[0].rows, inputs[0].cols, 1.0, inputs[0].data, inputs[0].rows,
            inputs[1].data, 1, 1.0, output.data, 1);
        break;
    }
    case Syrk_N: {
        std::copy(inputs[1].data, inputs[1].data + (output.rows * output.cols), output.data);
        cblas_dsyrk(
            CblasColMajor, CblasUpper, CblasNoTrans, output.rows, inputs[0].cols, 1.0, inputs[0].data, inputs[0].rows,
            1.0, output.data, output.rows);

        // Fill the lower triangular part of the matrix to make it symmetric
        for (int j = 0; j < output.cols; ++j) {
            for (int i = j + 1; i < output.rows; ++i) {
                output.data[i + j * output.rows] = output.data[j + i * output.rows];
            }
        }
        break;
    }
    case Syrk_T: {
        std::copy(inputs[1].data, inputs[1].data + (output.rows * output.cols), output.data);
        cblas_dsyrk(
            CblasColMajor, CblasUpper, CblasTrans, output.rows, inputs[0].rows, 1.0, inputs[0].data, inputs[0].rows,
            1.0, output.data, output.rows);
        for (int j = 0; j < output.cols; ++j) {
            for (int i = j + 1; i < output.rows; ++i) {
                output.data[i + j * output.rows] = output.data[j + i * output.rows];
            }
        }
        break;
    }
    case Trtri: {
        int n = output.rows;
        std::copy(inputs[0].data, inputs[0].data + n * n, output.data);
        auto is_lower = egraph.get_class_analysis_data(node->get_children()[0]);
        char uplo = std::get<MatrixProperty>(is_lower.property).flags.is_lower_triangular ? 'L' : 'U';
        LAPACKE_dtrtri(LAPACK_COL_MAJOR, uplo, 'N', n, output.data, n);
        break;
    }

        // normally should be consumed as kernel parameters
    case Tr: {
        int rows = inputs[0].rows;
        int cols = inputs[0].cols;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                // Column major: input[i, j] = input.data[i + j * rows]
                // Column major: output[j, i] = output.data[j + i * cols]
                output.data[j + i * cols] = inputs[0].data[i + j * rows];
            }
        }
        break;
    }
    case Gemm_NN:
    case Gemm_NT:
    case Gemm_TN:
    case Gemm_TT: {
        throw std::runtime_error("Kernel not implemented for this operation: " + std::to_string(static_cast<int>(op)));
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
        std::copy(input.data, input.data + (res.rows * res.cols), res.data);
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'U', res.rows, res.data, res.rows);
        // Zero out the lower triangular part
        for (int j = 0; j < res.cols; ++j) {
            for (int i = j + 1; i < res.rows; ++i) {
                res.data[i + j * res.rows] = 0.0;
            }
        }
        break;
    }
    case Potrf_L: {
        MatrixNode &res = output.matrices[0];
        std::copy(input.data, input.data + (res.rows * res.cols), res.data);
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', res.rows, res.data, res.rows);
        for (int j = 0; j < res.cols; ++j) {
            for (int i = 0; i < j; ++i) {
                res.data[i + j * res.rows] = 0.0;
            }
        }
        break;
    }
    case Geqrf: {
        double *a_copy = new double[input.rows * input.cols];
        std::copy(input.data, input.data + (input.rows * input.cols), a_copy);
        LAPACKE_dgeqrf(LAPACK_COL_MAJOR, input.rows, input.cols, a_copy, input.rows, output.tau);

        // Fill the R matrix (upper triangular) in the output tuple
        if (output.matrices.size() > 1) {
            MatrixNode &r_node = output.matrices[1];
            for (int j = 0; j < r_node.cols; ++j) {
                for (int i = 0; i < r_node.rows; ++i) {
                    if (i > j) {
                        r_node.data[i + j * r_node.rows] = 0.0;
                    } else {
                        r_node.data[i + j * r_node.rows] = a_copy[i + j * input.rows];
                    }
                }
            }
        }
        if (!output.matrices.empty()) {
            MatrixNode &q_node = output.matrices[0];
            int k = std::min(input.rows, input.cols);
            // Copy the reflectors part of a_copy to q_node before generating Q
            for (int j = 0; j < q_node.cols; ++j) {
                for (int i = 0; i < q_node.rows; ++i) {
                    q_node.data[i + j * q_node.rows] = a_copy[i + j * input.rows];
                }
            }
            LAPACKE_dorgqr(LAPACK_COL_MAJOR, q_node.rows, q_node.cols, k, q_node.data, q_node.rows, output.tau);
        }
        delete[] a_copy;
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
    std::copy(source.data, source.data + (output.rows * output.cols), output.data);
}
