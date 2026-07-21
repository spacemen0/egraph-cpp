#include "e_graph.h"
#include "extractor.h"

struct NodeData {
    double *data; // will use random data for testing
    int rows;
    int cols;
};

class Evaluator {
  public:
    explicit Evaluator(EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings);
    double *evaluate();

  private:
    void dispatch_kernel(const Op &op, std::vector<NodeData> inputs, NodeData &output) const;
    EGraph &egraph;
    ExtractionResult result;
    std::unordered_map<Id, NodeData> data_storage;
};