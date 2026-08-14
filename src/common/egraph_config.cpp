#include "egraph_config.h"
#include "expression.h"
#include <algorithm>

void initialize_config_for_expression(EGraphConfig &config, const Expression &expr) {
    size_t depth = expr.depth();
    size_t nodes = expr.node_count();

    // Scale rewriter node capacity based on initial AST size
    config.rewrite.node_limit = std::max(config.rewrite.node_limit, std::max(static_cast<size_t>(5000), nodes * 300));

    // Scale iteration depth based on AST complexity
    config.rewrite.max_iterations =
        std::max(config.rewrite.max_iterations, std::max(static_cast<size_t>(8), depth * 2));
    config.pruner.num_iterations =
        std::max(config.pruner.num_iterations, static_cast<int>(std::max(static_cast<size_t>(6), depth)));
    config.pruner.rewrite_steps_per_iteration =
        std::max(config.pruner.rewrite_steps_per_iteration, static_cast<int>(std::min(static_cast<size_t>(8), depth)));
}

EGraphConfig initialize_config_for_expression(const Expression &expr) {
    EGraphConfig config;
    initialize_config_for_expression(config, expr);
    return config;
}
