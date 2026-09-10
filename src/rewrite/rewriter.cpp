#include "rewriter.h"
#include "matcher.h"
#include "rewrite_sets.h"
#include "utils.h"
#include <iostream>

// Instantiate a pattern into the EGraph

namespace egraph {

static inline bool is_primitive_math_op(Op op) {
    using enum Op;
    return op == Add || op == Mul || op == Minus || op == Tr || op == Inv || op == Get;
}

static inline bool is_symbolic_op(Op op) { return !is_kernel_op(op) && !is_primitive_math_op(op); }

std::unordered_set<Op> Rewriter::compute_effective_disabled_ops(
    const std::vector<Op> &disabled_ops, const std::vector<Rewrite> *rules_to_inspect) {
    if (disabled_ops.empty()) {
        return {};
    }

    std::unordered_set<Op> effective_disabled_operations(disabled_ops.begin(), disabled_ops.end());

    // Map each symbolic operation to its list of alternative lowering paths.
    // Each lowering path is a set of ops (kernels or symbolic ops) required to lower it.
    std::unordered_map<Op, std::vector<std::unordered_set<Op>>> symbolic_op_to_alternative_lowering_paths;

    const auto &candidate_rules = rules_to_inspect ? *rules_to_inspect : build_complete_rewrite_set();

    for (const auto &rule : candidate_rules) {
        std::unordered_set<Op> lhs_ops;
        std::unordered_set<Op> rhs_ops;
        rule.lhs.collect_ops(lhs_ops);
        rule.collect_rhs_ops(rhs_ops);

        for (Op symbolic_op : lhs_ops) {
            // A rule is a lowering path when sym op appears on lhs and is eliminated on rhs.
            if (is_symbolic_op(symbolic_op) && !rhs_ops.contains(symbolic_op)) {
                std::unordered_set<Op> required_ops_in_path;
                for (Op rhs_op : rhs_ops) {
                    if (is_kernel_op(rhs_op) || is_symbolic_op(rhs_op)) {
                        required_ops_in_path.insert(rhs_op);
                    }
                }
                if (!required_ops_in_path.empty()) {
                    symbolic_op_to_alternative_lowering_paths[symbolic_op].push_back(std::move(required_ops_in_path));
                }
            }
        }
    }

    // A single lowering path is blocked if any of its required opes is disabled.
    // A symbolic operation is blocked if all of its alternative lowering paths are blocked.
    bool has_newly_disabled_operation = true;
    while (has_newly_disabled_operation) {
        has_newly_disabled_operation = false;
        for (const auto &[symbolic_op, alternative_paths] : symbolic_op_to_alternative_lowering_paths) {
            if (effective_disabled_operations.contains(symbolic_op)) {
                continue;
            }
            bool are_all_alternative_paths_blocked = true;
            for (const auto &required_ops_in_path : alternative_paths) {
                bool is_current_path_blocked = false;
                for (Op required_op : required_ops_in_path) {
                    if (effective_disabled_operations.contains(required_op)) {
                        is_current_path_blocked = true;
                        break;
                    }
                }
                if (!is_current_path_blocked) {
                    are_all_alternative_paths_blocked = false;
                    break;
                }
            }
            if (are_all_alternative_paths_blocked) {
                effective_disabled_operations.insert(symbolic_op);
                has_newly_disabled_operation = true;
            }
        }
    }

    return effective_disabled_operations;
}

Rewriter::Rewriter(EGraph &egraph, std::vector<Rewrite> rewrites, const EGraphConfig &config)
    : egraph(egraph), config(config), enable_backoff(config.rewrite.enable_backoff),
      enable_node_limit(config.rewrite.enable_node_limit), max_nodes(config.rewrite.node_limit),
      all_rewrites(std::move(rewrites)), rewrites(all_rewrites) {
    std::cout << "[Rewriter] Initialized with " << this->all_rewrites.size() << " rewrites.\n";
    filter_rewrites_by_disabled_ops();
    std::cout << "[Rewriter] After filtering, " << this->rewrites.size() << " rewrites remain.\n";
    reset_limits_and_bans();
}

void Rewriter::reset_limits_and_bans() {
    current_match_limits.resize(rewrites.size());
    rewrite_application_counts.assign(rewrites.size(), 0);
    ban_iterations_remaining.assign(rewrites.size(), 0);
    ban_duration_next.assign(rewrites.size(), 1);

    std::ranges::transform(rewrites, current_match_limits.begin(), [](const auto &r) {
        return r.initial_match_limit;
    });
}

void Rewriter::set_config(const EGraphConfig &cfg) {
    config = cfg;
    enable_backoff = cfg.rewrite.enable_backoff;
    enable_node_limit = cfg.rewrite.enable_node_limit;
    max_nodes = cfg.rewrite.node_limit;
    filter_rewrites_by_disabled_ops();
    reset_limits_and_bans();
}

void Rewriter::filter_rewrites_by_disabled_ops() {
    rewrites = all_rewrites;
    auto effective_disabled_operations = compute_effective_disabled_ops(config.disabled_ops);
    if (effective_disabled_operations.empty()) {
        return;
    }
    rewrites.erase(
        std::remove_if(
            rewrites.begin(), rewrites.end(),
            [&effective_disabled_operations](const Rewrite &rule) {
        return std::any_of(
            effective_disabled_operations.begin(), effective_disabled_operations.end(), [&rule](const Op &disabled_op) {
            return rule.contains_op(disabled_op);
        });
    }),
        rewrites.end());
}

static Id instantiate(EGraph &egraph, const Pattern &pattern, const Substitution &subst) {
    if (const auto *str = std::get_if<uint32_t>(&pattern.atom)) {
        if (get_string_from_lookup(*str).starts_with('?')) {
            return subst.at(get_string_from_lookup(*str).substr(1));
        }
    }

    Children children;
    children.reserve(pattern.children.size());
    for (const auto &child_pat : pattern.children) {
        children.emplace_back(instantiate(egraph, child_pat, subst));
    }
    ENode node(children, pattern.atom);
    return egraph.add_node(node); // new id or existing id
}

bool Rewriter::is_rewrite_banned(size_t i) {
    if (ban_iterations_remaining[i] > 0 && enable_backoff) {
        ban_iterations_remaining[i]--;
        return true;
    }
    return false;
}

void Rewriter::update_ban_status(size_t i, size_t total_valid_matches, size_t budget_remaining) {
    if (total_valid_matches > budget_remaining && enable_backoff) {
        ban_iterations_remaining[i] = ban_duration_next[i];
        ban_duration_next[i] *= 2;
        current_match_limits[i] *= 2;
    } else if (enable_backoff) {
        // if the rewrite was not banned, we reduce the ban duration for next time
        ban_duration_next[i] = std::max(size_t(1), ban_duration_next[i] / 2);
    }
}

std::vector<Rewriter::Match> Rewriter::find_matches_for_rewrite(
    size_t i, const Matcher &matcher, const std::vector<Id> &class_ids, size_t &total_valid_matches) {
    auto &rewrite = rewrites[i];
    std::vector<Match> rewrite_matches;
    total_valid_matches = 0;

    for (Id class_id : class_ids) {
        // Only check root classes
        if (egraph.find_class_id(class_id) != class_id)
            continue;

        std::set<Substitution> substs = matcher.find_matches_in_eclass(class_id, rewrite.lhs);
        std::set<Substitution> reverse_substs;
        if (rewrite.bidirectional) {
            reverse_substs = matcher.find_matches_in_eclass(class_id, rewrite.rhs);
        }
        for (const auto &subst : substs) {
            if (rewrite.condition && !rewrite.condition(egraph, subst)) {
                continue;
            }
            total_valid_matches++;
            rewrite_matches.emplace_back(class_id, i, subst, true);
        }
        for (const auto &subst : reverse_substs) {
            if (rewrite.condition && !rewrite.condition(egraph, subst)) {
                continue;
            }
            total_valid_matches++;
            rewrite_matches.emplace_back(class_id, i, subst, false);
        }
    }
    return rewrite_matches;
}

bool Rewriter::apply_matches(const std::vector<Match> &matches) {
    bool changed = false;
    for (const auto &match : matches) {
        const auto &rewrite = rewrites[match.rewrite_idx];
        if (rewrite.applier) {
            auto result = rewrite.applier(egraph, match.subst, match.class_id);
            changed |= egraph.union_classes(match.class_id, result.first);
            changed |= result.second;
        } else if (match.left_to_right) {
            changed |= egraph.union_classes(match.class_id, instantiate(egraph, rewrite.rhs, match.subst));
        } else {
            changed |= egraph.union_classes(match.class_id, instantiate(egraph, rewrite.lhs, match.subst));
        }

        if (enable_node_limit && egraph.num_nodes() > max_nodes) {
            break; // Break the apply loop to enforce limits but guarantee rebuild
        }
    }
    return changed;
}

bool Rewriter::apply_one_iteration() {
    bool changed = false;

    Matcher matcher(egraph);

    // Store matches to apply them in batch: (class_id, rewrite_index,
    // substitution)
    std::vector<Match> matches;

    std::vector<Id> class_ids = egraph.get_all_class_ids();
    std::ranges::sort(class_ids);

    for (size_t i = 0; i < rewrites.size(); ++i) {
        if (is_rewrite_banned(i)) {
            continue;
        }

        rewrite_application_counts[i] = 0;
        size_t total_valid_matches = 0;
        std::vector<Match> rewrite_matches = find_matches_for_rewrite(i, matcher, class_ids, total_valid_matches);

        size_t budget_remaining = enable_backoff ? current_match_limits[i] : total_valid_matches;
        size_t matches_to_apply = std::min(total_valid_matches, budget_remaining);

        if (total_valid_matches <= budget_remaining) {
            for (size_t j = 0; j < total_valid_matches; ++j) {
                matches.push_back(rewrite_matches[j]);
            }
        } else {
            // evenly distribute matches from the beginning and end of the list
            size_t half = budget_remaining / 2;
            for (size_t j = 0; j < half; ++j) {
                matches.push_back(rewrite_matches[j]);
            }
            for (size_t j = total_valid_matches - (budget_remaining - half); j < total_valid_matches; ++j) {
                matches.push_back(rewrite_matches[j]);
            }
        }
        rewrite_application_counts[i] += matches_to_apply;

        update_ban_status(i, total_valid_matches, budget_remaining);
    }

    changed = apply_matches(matches);

    if (changed) {
        egraph.rebuild();
    }

    if (enable_node_limit && egraph.num_nodes() > max_nodes) {
        std::cout << "[Rewriter] Node limit exceeded: " << max_nodes << "\n";
        return false;
    }

    return changed;
}

bool Rewriter::apply_rewrites(int max_iterations) {
    bool any_changed = false;

    for (int i = 0; i < max_iterations; ++i) {

        if (!apply_one_iteration()) {
            break;
        }
        any_changed = true;
    }

    return any_changed;
}

/// @brief Apply rewrites until saturation
/// @return
bool Rewriter::apply_rewrites() {
    bool changed = false;
    int iteration = 0;
    while (true) {
        if (!apply_one_iteration()) {
            break;
        }
        changed = true;
        iteration++;
    }
    return changed;
}

void Rewriter::reset() {
    std::fill(rewrite_application_counts.begin(), rewrite_application_counts.end(), 0);
    std::fill(ban_iterations_remaining.begin(), ban_iterations_remaining.end(), 0);
    std::fill(ban_duration_next.begin(), ban_duration_next.end(), 1);
    std::ranges::transform(rewrites, current_match_limits.begin(), [](const auto &r) {
        return r.initial_match_limit;
    });
}

} // namespace egraph
