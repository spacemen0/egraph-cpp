#include "e_graph.h"
#include "extractor.h"

struct NodeData {
    double *data; // will use random data for testing
    int rows;
    int cols;
    double *tau = nullptr; // Used for LAPACK QR factorization
};

class Evaluator {
  public:
    explicit Evaluator(EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings);
    double *evaluate();

  private:
    void dispatch_kernel(Op op, const std::vector<NodeData> &inputs, NodeData &output, const ENode *node) const;
    EGraph &egraph;
    ExtractionResult result;
    std::unordered_map<Id, NodeData> data_storage;
};