#pragma once

#include <cstddef>

namespace egraph {
struct RewriteConfig {
    size_t node_limit = 5000;
    size_t max_iterations = 10;
    bool enable_backoff = true;
    bool enable_node_match_limit = false;
};

struct ExtractorConfig {
    size_t max_depth = 40;
    size_t node_visit_limit = 10000000;
};

struct PrunerConfig {
    int num_iterations = 5;
    int rewrite_steps_per_iteration = 6;
    int prune_samples_per_iteration = 5;
    int max_results_per_binding = 10;
};

struct EGraphConfig {
    RewriteConfig rewrite;
    ExtractorConfig extractor;
    PrunerConfig pruner;
    bool enable_logging = false;
    void print_config();
};

struct Expression;

void initialize_config_for_expression(EGraphConfig &config, const Expression &expr);

EGraphConfig initialize_config_for_expression(const Expression &expr);

} // namespace egraph
