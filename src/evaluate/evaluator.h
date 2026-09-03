#pragma once
#include "e_graph.h"
#include "extractor.h"

#include <memory>

namespace egraph {
enum class StorageFormat { General, SymmetricUpper, SymmetricLower, TriangularUpper, TriangularLower };

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

using DataStorage = std::variant<MatrixNode, TupleNode>;

class Evaluator {
  public:
    explicit Evaluator(
        EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings,
        const DataBindings &data_bindings);
    std::vector<double> evaluate();

  private:
    void dispatch_matrix_kernel(Op op, MatrixNode &output, const ENode *node, Id class_id) const;
    void dispatch_factorization(Op op, const MatrixNode &input, TupleNode &output, const ENode *node) const;
    void setup_in_place_output(Id child_id, MatrixNode &output) const;
    void dispatch_get(const TupleNode &input_tuple, int index, MatrixNode &output) const;
    EGraph &egraph;
    ExtractionResult result;
    const DataBindings &data_bindings;

    // element 0 stores the data for the first node in execution_order, and so on.
    std::vector<DataStorage> data_storage;
    // A mapping from class_id to the index in execution_order
    std::vector<int> slot_map;
    // index is also the slot in execution_order (stores how many times a node will be referenced in the execution
    // chain.)
    mutable std::vector<int> use_counts;
    std::unordered_map<Id, bool> prefer_upper_triangular;
};
} // namespace egraph
