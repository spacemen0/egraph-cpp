#include "rewriter.h"
#include "matcher.h"
#include "cost_storage.h"
#include <iostream>
#include <limits>

using Match = struct
{
    Id class_id;
    size_t rewrite_idx;
    Substitution subst;
};

// Instantiate a pattern into the EGraph
static Id instantiate(EGraph &egraph, const Pattern &pattern, const Substitution &subst)
{
    if (const auto *str = std::get_if<std::string>(&pattern.atom))
    {
        if (str->starts_with('?'))
        {
            return subst.at(str->substr(1));
        }
    }

    Children children;
    children.reserve(pattern.children.size());
    for (const auto &child_pat : pattern.children)
    {
        children.emplace_back(instantiate(egraph, child_pat, subst));
    }
    ENode node(children, pattern.atom);
    return egraph.add_node(node); // new id or existing id
}

bool Rewriter::apply_one_iteration(size_t node_match_limit)
{
    bool changed = false;

    Matcher matcher(egraph);

    // Store matches to apply them in batch: (class_id, rewrite_index, substitution)
    std::vector<Match> matches;

    std::vector<Id> class_ids = egraph.get_all_class_ids();

    for (size_t i = 0; i < rewrites.size(); ++i)
    {
        if (ban_iterations_remaining[i] > 0 && enable_backoff)
        {
            ban_iterations_remaining[i]--;
            if (ban_iterations_remaining[i] == 0)
            {
                rewrite_application_counts[i] = 0;
            }
            continue;
        }

        auto &rewrite = rewrites[i];
        std::vector<Match> rewrite_matches;
        size_t total_valid_matches = 0;

        for (Id class_id : class_ids)
        {
            // Only check root classes
            if (egraph.find_class_id(class_id) != class_id)
                continue;

            std::set<Substitution> substs = matcher.find_matches_in_eclass(class_id, rewrite.lhs, node_match_limit);

            for (const auto &subst : substs)
            {
                if (rewrite.condition && !rewrite.condition(egraph, subst))
                {
                    continue;
                }
                total_valid_matches++;
                rewrite_matches.emplace_back(class_id, i, subst);
            }
        }

        size_t budget_remaining = enable_backoff ? current_match_limits[i] - rewrite_application_counts[i] : total_valid_matches;
        size_t matches_to_apply = std::min(total_valid_matches, budget_remaining);

        for (size_t j = 0; j < matches_to_apply; ++j)
        {
            matches.push_back(rewrite_matches[j]);
        }
        rewrite_application_counts[i] += matches_to_apply;

        if (total_valid_matches > budget_remaining && enable_backoff)
        {
            ban_iterations_remaining[i] = ban_duration_next[i];
            ban_duration_next[i] *= 2;
            current_match_limits[i] *= 2;

            std::cout << "Rewrite '" << rewrite.name << "' exceeded match limit ("
                      << total_valid_matches << " matches found, budget " << budget_remaining
                      << "). Applied " << matches_to_apply << ". Banning for " << ban_iterations_remaining[i]
                      << " iterations. New limit: " << current_match_limits[i] << std::endl;
        }
    }

    for (const auto &match : matches)
    {
        const auto &rewrite = rewrites[match.rewrite_idx];

        Id rhs_id;
        if (rewrite.applier)
        {
            rhs_id = rewrite.applier(egraph, match.subst);
        }
        else
        {
            rhs_id = instantiate(egraph, rewrite.rhs, match.subst);
        }

        if (egraph.union_classes(match.class_id, rhs_id))
        {
            changed = true;
        }
        if (egraph.num_nodes() > max_nodes)
        {
            return false;
        }
    }

    if (changed)
    {
        egraph.rebuild();
        cost_storage.compute();
    }

    return changed;
}

bool Rewriter::apply_rewrites(int max_iterations)
{
    bool any_changed = false;

    for (int i = 0; i < max_iterations; ++i)
    {
        size_t node_match_limit = 0;
        if (enable_node_match_limit)
        {
            node_match_limit = (i % 2 == 1) ? 0 : 1;
        }

        if (!apply_one_iteration(node_match_limit))
        {
            break;
        }
        any_changed = true;
    }

    return any_changed;
}

/// @brief Apply rewrites until saturation
/// @return
bool Rewriter::apply_rewrites()
{
    bool changed = false;
    int iteration = 0;
    while (true)
    {
        size_t node_match_limit = 0;
        if (enable_node_match_limit)
        {
            node_match_limit = (iteration % 2 == 1) ? 0 : 1;
        }

        if (!apply_one_iteration(node_match_limit))
        {
            break;
        }
        changed = true;
        iteration++;
    }
    return changed;
}
