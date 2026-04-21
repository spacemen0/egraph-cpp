#pragma once
#include "cost_storage.h"
#include "e_graph.h"
#include "pattern.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

struct Rewrite {
    std::string name;
    Pattern lhs;
    Pattern rhs;
    bool bidirectional = false;
    std::function<bool(const EGraph &, const Substitution &)> condition = nullptr;
    std::function<Id(EGraph &, const Substitution &)> applier = nullptr;
    size_t initial_match_limit = 30;
};

class Rewriter {
  public:
    Rewriter(
        EGraph &egraph, std::vector<Rewrite> rewrites, size_t max_nodes, bool enable_backoff = false,
        bool enable_node_match_limit = false)
        : egraph(egraph), rewrites(std::move(rewrites)), max_nodes(max_nodes), enable_backoff(enable_backoff),
          enable_node_match_limit(enable_node_match_limit) {
        current_match_limits.resize(this->rewrites.size());
        rewrite_application_counts.resize(this->rewrites.size(), 0);
        ban_iterations_remaining.resize(this->rewrites.size(), 0);
        ban_duration_next.resize(this->rewrites.size(), 1);

        std::ranges::transform(this->rewrites, current_match_limits.begin(), [](const auto &r) {
            return r.initial_match_limit;
        });
    }
    bool apply_one_iteration(size_t node_match_limit = 0);
    bool apply_rewrites(int max_iterations);
    bool apply_rewrites();
    void reset();

  private:
    EGraph &egraph;
    bool enable_backoff;
    bool enable_node_match_limit;
    std::vector<Rewrite> rewrites;
    std::vector<size_t> current_match_limits;       // Current limit (doubles when banning)
    std::vector<size_t> rewrite_application_counts; // Accumulated applications in
                                                    // current iteration
    std::vector<size_t> ban_iterations_remaining;   // How many iterations left in current ban
    std::vector<size_t> ban_duration_next;          // Duration for next ban (starts at 1, doubles)
    size_t max_nodes;
};
