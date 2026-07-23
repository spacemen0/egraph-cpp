#include "evaluator.h"
#include "utils.h"
#include <cstdint>
#include <variant>
#include <vector>

#ifdef __APPLE__
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
                NodeData node_data;
                Shape shape = bind_shape(data->shape, size_bindings);
                node_data.rows = *std::get_if<int>(&shape.first);
                node_data.cols = *std::get_if<int>(&shape.second);
                if (std::holds_alternative<Op>(atom)) // op
                {
                    node_data.data = new double[node_data.rows * node_data.cols];
                }
                if (std::holds_alternative<uint32_t>(atom)) // matrix
                {
                    node_data.data = new double[node_data.rows * node_data.cols];
                    for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                        node_data.data[i] = static_cast<double>(rand()) / RAND_MAX; // Random values
                    }
                }
                data_storage[class_id] = node_data;
            } else if (std::holds_alternative<double>(atom)) {
                NodeData node_data;
                node_data.rows = 1;
                node_data.cols = 1;
                node_data.data = new double{std::get<double>(atom)};
                data_storage[class_id] = node_data;
            } else if (const auto *op = std::get_if<Op>(&atom)) {
                if (*op == Op::Geqrf || *op == Op::Potrf_L || *op == Op::Potrf_U) {
                    Id child_id = node->get_children()[0];
                    const NodeData &child = data_storage.at(child_id);
                    NodeData node_data;
                    node_data.rows = child.rows;
                    node_data.cols = child.cols;
                    node_data.data = new double[node_data.rows * node_data.cols];
                    node_data.tau = new double[std::min(node_data.rows, node_data.cols)];
                    data_storage[class_id] = node_data;
                }
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
                    NodeData &node_data = it->second;
                    std::vector<NodeData> inputs;
                    for (Id child_id : node->get_children()) {
                        inputs.push_back(data_storage.at(child_id));
                    }
                    dispatch_kernel(*op, inputs, node_data, node);
                }
            }
        }
    }
    return data_storage.at(result.execution_order.back()).data;
}

void Evaluator::dispatch_kernel(Op op, const std::vector<NodeData> &inputs, NodeData &output, const ENode *node) const {
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
    case Geqrf: {
        std::copy(inputs[0].data, inputs[0].data + (output.rows * output.cols), output.data);
        LAPACKE_dgeqrf(LAPACK_COL_MAJOR, output.rows, output.cols, output.data, output.rows, output.tau);
        break;
    }
    case Get: {
        int index = static_cast<int>(*inputs[1].data);
        Id tuple_id = node->get_children()[0];
        const ENode *tuple_node = result.choices.at(tuple_id);
        Atom tuple_atom = tuple_node->get_atom();
        const auto *tuple_op = std::get_if<Op>(&tuple_atom);

        if (tuple_op && *tuple_op == Op::Geqrf) {
            if (index == 0) {
                std::copy(inputs[0].data, inputs[0].data + (inputs[0].rows * inputs[0].cols), output.data);
                int k = std::min(inputs[0].rows, inputs[0].cols);
                LAPACKE_dorgqr(LAPACK_COL_MAJOR, output.rows, output.cols, k, output.data, output.rows, inputs[0].tau);
            } else if (index == 1) {
                int src_rows = inputs[0].rows;
                for (int j = 0; j < output.cols; ++j) {
                    for (int i = 0; i < output.rows; ++i) {
                        if (i > j) {
                            output.data[j * output.rows + i] = 0.0;
                        } else {
                            output.data[j * output.rows + i] = inputs[0].data[j * src_rows + i];
                        }
                    }
                }
            }
        } else {
            // For Potrf_U, Potrf_L, just copy raw data
            std::copy(inputs[0].data, inputs[0].data + (inputs[0].rows * inputs[0].cols), output.data);
        }
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
    case Potrf_U: {
        std::copy(inputs[0].data, inputs[0].data + (output.rows * output.cols), output.data);
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'U', output.rows, output.data, output.rows);
        // Zero out the lower triangular part of the matrix (can be omitted)
        for (int j = 0; j < output.cols; ++j) {
            for (int i = j + 1; i < output.rows; ++i) {
                output.data[i + j * output.rows] = 0.0;
            }
        }
        break;
    }
    case Potrf_L: {
        std::copy(inputs[0].data, inputs[0].data + (output.rows * output.cols), output.data);
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', output.rows, output.data, output.rows);
        for (int j = 0; j < output.cols; ++j) {
            for (int i = 0; i < j; ++i) {
                output.data[i + j * output.rows] = 0.0;
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
