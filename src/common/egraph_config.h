#pragma once

#include <cstddef>
#include <string>
#include <vector>

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
    int num_iterations = 8;
    int rewrite_steps_per_iteration = 6;
    int prune_samples_per_iteration = 5;
    int max_results_per_binding = 5;
    std::vector<std::string> size_keys;
};

struct EGraphConfig {
    RewriteConfig rewrite;
    ExtractorConfig extractor;
    PrunerConfig pruner;
    bool enable_logging = false;
};
