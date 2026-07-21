#include "evaluator.h"
#include "utils.h"
#include <variant>

Evaluator::Evaluator(EGraph &egraph, const ExtractionResult &result, const SizeBindings *size_bindings)
    : egraph(egraph), result(result) {
    // Initialize data storage for each node in the extraction result
    for (Id class_id : result.execution_order) {
        const ENode *node = result.choices.at(class_id);
        if (node) {
            const Atom &atom = node->get_atom();
            if (const auto *op = std::get_if<Op>(&atom)) {
                // For matrix operations, allocate random data based on the shape
                if (auto data = get_matrix_data(egraph, class_id)) {
                    if (data->has_symbolic_shape() && (!size_bindings || size_bindings->empty())) {
                        throw std::runtime_error("Cannot evaluate with symbolic shapes without size bindings.");
                    }
                    NodeData node_data;
                    Shape shape = bind_shape(data->shape, size_bindings);
                    node_data.rows = *std::get_if<int>(&shape.first);
                    node_data.cols = *std::get_if<int>(&shape.second);
                    node_data.data = new double[node_data.rows * node_data.cols];
                    for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                        node_data.data[i] = static_cast<double>(rand()) / RAND_MAX; // Random values
                    }
                    data_storage[class_id] = node_data;
                }
            }
        }
    }
}