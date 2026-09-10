#pragma once
#include "e_graph.h"
#include "egraph_config.h"
#include "pattern.h"
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
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
    std::vector<Op> dynamic_ops = {};

    void collect_rhs_ops(std::unordered_set<Op> &ops) const {
        rhs.collect_ops(ops);
        for (Op op : dynamic_ops) {
            ops.insert(op);
        }
    }

    bool contains_op(const Op &op) const {
        if (lhs.contains_op(op) || rhs.contains_op(op)) {
            return true;
        }
        return std::find(dynamic_ops.begin(), dynamic_ops.end(), op) != dynamic_ops.end();
    }
};

class Rewriter {
  public:
    Rewriter(EGraph &egraph, std::vector<Rewrite> rewrites, const EGraphConfig &config = EGraphConfig());

    static std::unordered_set<Op> compute_effective_disabled_ops(
        const std::vector<Op> &disabled_ops,
        const std::vector<Rewrite> *custom_lowering_rules = nullptr);

    void set_config(const EGraphConfig &cfg);
    const EGraphConfig &get_config() const { return config; }

    bool apply_rewrites(int max_iterations);
    bool apply_rewrites();
    void reset();
    void reset_limits_and_bans();

    const std::vector<Rewrite> &get_rewrites() const { return rewrites; }
    const std::vector<Rewrite> &get_all_rewrites() const { return all_rewrites; }

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
    std::vector<Rewrite> all_rewrites;
    std::vector<Rewrite> rewrites;
    std::vector<size_t> current_match_limits;       // Current limit (doubles when banning) (for backoff)
    std::vector<size_t> rewrite_application_counts; // Accumulated applications in
                                                    // current iteration
    std::vector<size_t> ban_iterations_remaining;   // How many iterations left in current ban
    std::vector<size_t> ban_duration_next;          // Duration for next ban (starts at 1, doubles)
    size_t max_nodes;
};

} // namespace egraph
