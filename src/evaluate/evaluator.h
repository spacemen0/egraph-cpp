#pragma once
#include "e_graph.h"
#include "extractor.h"

#include <memory>

struct MatrixNode {
    std::shared_ptr<std::vector<double>> data_ptr;
    int rows = 0;
    int cols = 0;

    MatrixNode() = default;
    MatrixNode(int r, int c) : data_ptr(std::make_shared<std::vector<double>>(r * c)), rows(r), cols(c) {}
    MatrixNode(int r, int c, std::vector<double> vec)
        : data_ptr(std::make_shared<std::vector<double>>(std::move(vec))), rows(r), cols(c) {}

    const double *data() const { return data_ptr ? data_ptr->data() : nullptr; }
    double *data() { return data_ptr ? data_ptr->data() : nullptr; }
    const std::vector<double> &vec() const { return *data_ptr; }
    std::vector<double> &vec() { return *data_ptr; }
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
    void dispatch_matrix_kernel(Op op, MatrixNode &output, const ENode *node) const;
    void dispatch_factorization(Op op, const MatrixNode &input, TupleNode &output, const ENode *node) const;
    void setup_in_place_output(Id child_id, MatrixNode &output) const;
    void dispatch_get(const TupleNode &input_tuple, int index, MatrixNode &output) const;
    EGraph &egraph;
    ExtractionResult result;
    std::unordered_map<Id, DataStorage> data_storage;
    mutable std::unordered_map<Id, int> use_counts;
};