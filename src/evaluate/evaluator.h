#pragma once
#include "e_graph.h"
#include "extractor.h"

struct MatrixNode {
    std::vector<double> raw_data_vector; // will use random data for testing
    int rows;
    int cols;
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
        const DataBindings *data_bindings = nullptr);
    std::vector<double> evaluate();

  private:
    void
    dispatch_matrix_kernel(Op op, const std::vector<MatrixNode> &inputs, MatrixNode &output, const ENode *node) const;
    void dispatch_factorization(Op op, const MatrixNode &input, TupleNode &output, const ENode *node) const;
    void dispatch_get(const TupleNode &input_tuple, int index, MatrixNode &output) const;
    EGraph &egraph;
    ExtractionResult result;
    std::unordered_map<Id, DataStorage> data_storage;
};