#pragma once

#include <cstddef>
struct EGraphConfig {
    size_t node_limit = 5000;
    size_t max_iterations = 10;
    size_t max_depth = 40;
    size_t node_visit_limit = 10000000;
    size_t prune_iterations = 8;
    bool enable_logging = false;
    bool enable_backoff = true;
    bool enable_node_match_limit = false;
};
