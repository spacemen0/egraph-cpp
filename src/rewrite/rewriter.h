#pragma once
#include "e_graph.h"
#include "egraph_config.h"
#include "pattern.h"
#include <functional>
#include <string>
#include <vector>

namespace egraph {
struct Rewrite {
    std::string name;
    Pattern lhs;
    Pattern rhs;
    bool bidirectional = false;
    std::function<bool(const EGraph &, const Substitution &)> condition = nullptr;
    // bool indicates if analysis data of any e-node changed
    std::function<std::pair<Id, bool>(EGraph &, const Substitution &, Id)> applier = nullptr;
    size_t initial_match_limit = 30;
};

class Rewriter {
  public:
    Rewriter(EGraph &egraph, std::vector<Rewrite> rewrites, const EGraphConfig &config = EGraphConfig());

    void set_config(const EGraphConfig &cfg) {
        config = cfg;
        enable_backoff = cfg.rewrite.enable_backoff;
        enable_node_limit = cfg.rewrite.enable_node_limit;
        max_nodes = cfg.rewrite.node_limit;
    }
    const EGraphConfig &get_config() const { return config; }

    bool apply_rewrites(int max_iterations);
    bool apply_rewrites();
    void reset();

  private:
    struct Match {
        Id class_id;
        size_t rewrite_idx;
        Substitution subst;
        bool left_to_right;
    };
    bool apply_one_iteration();
    void filter_rewrites_by_disabled_ops();
    bool is_rewrite_banned(size_t i);
    void update_ban_status(size_t i, size_t total_valid_matches, size_t budget_remaining);
    std::vector<Match> find_matches_for_rewrite(
        size_t i, const class Matcher &matcher, const std::vector<Id> &class_ids, size_t &total_valid_matches);
    bool apply_matches(const std::vector<Match> &matches);

    EGraph &egraph;
    EGraphConfig config;
    bool enable_backoff;
    bool enable_node_limit;
    std::vector<Rewrite> rewrites;
    std::vector<size_t> current_match_limits;       // Current limit (doubles when banning) (for backoff)
    std::vector<size_t> rewrite_application_counts; // Accumulated applications in
                                                    // current iteration
    std::vector<size_t> ban_iterations_remaining;   // How many iterations left in current ban
    std::vector<size_t> ban_duration_next;          // Duration for next ban (starts at 1, doubles)
    size_t max_nodes;
};

} // namespace egraph
