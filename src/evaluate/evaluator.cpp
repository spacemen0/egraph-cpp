#include "evaluator.h"
#include "utils.h"
#include <variant>

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
                if (atom.index() == 0) // op
                {
                    node_data.data = new double[node_data.rows * node_data.cols];
                }
                if (atom.index() == 1) // matrix
                {
                    node_data.data = new double[node_data.rows * node_data.cols];
                    for (int i = 0; i < node_data.rows * node_data.cols; ++i) {
                        node_data.data[i] = static_cast<double>(rand()) / RAND_MAX; // Random values
                    }
                }
                if (atom.index() == 2) // scalar
                {
                    node_data.data = new double{std::get<double>(atom)};
                }
                data_storage[class_id] = node_data;
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
                    dispatch_kernel(*op, inputs, node_data);
                }
            }
        }
    }
    return data_storage.at(result.execution_order.back()).data;
}