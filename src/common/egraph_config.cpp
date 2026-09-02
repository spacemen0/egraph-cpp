#include "egraph_config.h"
#include "expression.h"
#include <algorithm>

namespace egraph {
void initialize_config_for_expression(EGraphConfig &config, const Expression &expr) {
    size_t depth = expr.depth();
    size_t nodes = expr.node_count();

    // Node limit scales exponentially with AST depth (up to depth 8)
    size_t exponential_scale = static_cast<size_t>(std::pow(2.0, std::min(depth, size_t(8))));
    size_t computed_limit = nodes * exponential_scale * 8;
    config.rewrite.node_limit = std::max(computed_limit, static_cast<size_t>(5000));

    // Scale iteration depth based on AST complexity
    config.rewrite.max_iterations =
        std::max(config.rewrite.max_iterations, std::max(static_cast<size_t>(10), depth * 2));
    config.pruner.rewrite_steps_per_iteration = std::max(
        config.pruner.rewrite_steps_per_iteration,
        static_cast<int>(std::max(static_cast<size_t>(10), static_cast<size_t>(depth * 2))));
    config.pruner.prune_samples_per_iteration = std::max(
        config.pruner.prune_samples_per_iteration,
        static_cast<int>(std::max(static_cast<size_t>(10), static_cast<size_t>(depth * 3))));
}

EGraphConfig initialize_config_for_expression(const Expression &expr) {
    EGraphConfig config;
    initialize_config_for_expression(config, expr);
    return config;
}

void EGraphConfig::print_config() {
    std::cout << "EGraphConfig:\n";
    std::cout << "  RewriteConfig:\n";
    std::cout << "    node_limit: " << rewrite.node_limit << "\n";
    std::cout << "    max_iterations: " << rewrite.max_iterations << "\n";
    std::cout << "    enable_backoff: " << (rewrite.enable_backoff ? "true" : "false") << "\n";
    std::cout << "    enable_node_match_limit: " << (rewrite.enable_node_limit ? "true" : "false") << "\n";
    std::cout << "  ExtractorConfig:\n";
    std::cout << "    max_depth: " << extractor.max_depth << "\n";
    std::cout << "    node_visit_limit: " << extractor.node_visit_limit << "\n";
    std::cout << "  PrunerConfig:\n";
    std::cout << "    num_iterations: " << pruner.num_iterations << "\n";
    std::cout << "    rewrite_steps_per_iteration: " << pruner.rewrite_steps_per_iteration << "\n";
    std::cout << "    prune_samples_per_iteration: " << pruner.prune_samples_per_iteration << "\n";
}

} // namespace egraph
