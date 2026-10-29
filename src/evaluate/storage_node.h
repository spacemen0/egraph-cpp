#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace egraph {

enum class StorageFormat : std::uint8_t { General, SymmetricUpper, SymmetricLower, TriangularUpper, TriangularLower };

struct MatrixNode {
    std::shared_ptr<std::vector<double>> data_ptr;
    int rows = 0;
    int cols = 0;
    mutable StorageFormat format = StorageFormat::General;

    MatrixNode() = default;
    MatrixNode(int r, int c) : data_ptr(std::make_shared<std::vector<double>>(r * c)), rows(r), cols(c) {}
    MatrixNode(int r, int c, std::vector<double> vec)
        : data_ptr(std::make_shared<std::vector<double>>(std::move(vec))), rows(r), cols(c) {}
    MatrixNode(int r, int c, StorageFormat f)
        : data_ptr(std::make_shared<std::vector<double>>(r * c)), rows(r), cols(c), format(f) {}

    const double *data() const { return data_ptr ? data_ptr->data() : nullptr; }
    double *data() { return data_ptr ? data_ptr->data() : nullptr; }
    const std::vector<double> &vec() const { return *data_ptr; }
    std::vector<double> &vec() { return *data_ptr; }

    void ensure_general() const {
        if (format == StorageFormat::General)
            return;

        // Use non-const data pointer to fill the matrix
        double *mut_data = data_ptr ? data_ptr->data() : nullptr;
        if (!mut_data)
            return;

        if (format == StorageFormat::SymmetricUpper) {
            for (int i = 0; i < rows; ++i) {
                for (int j = 0; j < i; ++j) {
                    mut_data[i + j * rows] = mut_data[j + i * rows];
                }
            }
        } else if (format == StorageFormat::SymmetricLower) {
            for (int j = 0; j < cols; ++j) {
                for (int i = 0; i < j; ++i) {
                    mut_data[i + j * rows] = mut_data[j + i * rows];
                }
            }
        } else if (format == StorageFormat::TriangularUpper) {
            for (int i = 0; i < rows; ++i) {
                for (int j = 0; j < i; ++j) {
                    mut_data[i + j * rows] = 0.0;
                }
            }
        } else if (format == StorageFormat::TriangularLower) {
            for (int j = 0; j < cols; ++j) {
                for (int i = 0; i < j; ++i) {
                    mut_data[i + j * rows] = 0.0;
                }
            }
        }
        format = StorageFormat::General;
    }
};

struct TupleNode {
    std::vector<MatrixNode> matrices;
    std::vector<double> tau; // Used for LAPACK QR factorization
};

using EvalValue = std::variant<MatrixNode, TupleNode>;

inline const MatrixNode *as_matrix_node(const EvalValue *val) { return val ? std::get_if<MatrixNode>(val) : nullptr; }
inline MatrixNode *as_matrix_node(EvalValue *val) { return val ? std::get_if<MatrixNode>(val) : nullptr; }

inline const TupleNode *as_tuple_node(const EvalValue *val) { return val ? std::get_if<TupleNode>(val) : nullptr; }
inline TupleNode *as_tuple_node(EvalValue *val) { return val ? std::get_if<TupleNode>(val) : nullptr; }

} // namespace egraph
