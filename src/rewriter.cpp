#include "rewriter.h"
#include "matcher.h"
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

bool Rewriter::apply_one_iteration()
{
    bool changed = false;
    Matcher matcher(egraph);

    // Store matches to apply them in batch: (class_id, rewrite_index, substitution)
    std::vector<Match> matches;

    // Search phase: collect all matches for each rewrite across all e-classes
    std::vector<Id> class_ids = egraph.get_all_class_ids();

    for (size_t i = 0; i < rewrites.size(); ++i)
    {
        if (ban_iterations_remaining[i] > 0)
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

            std::set<Substitution> substs = matcher.find_matches_in_eclass(class_id, rewrite.lhs);

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

        if (rewrite_application_counts[i] + total_valid_matches > current_match_limits[i])
        {
            ban_iterations_remaining[i] = ban_duration_next[i];
            ban_duration_next[i] *= 2;
            current_match_limits[i] *= 2;

            std::cout << "Rewrite '" << rewrite.name << "' exceeded match limit ("
                      << (rewrite_application_counts[i] + total_valid_matches) << " > " << (current_match_limits[i] / 2)
                      << "). Banning for " << ban_iterations_remaining[i]
                      << " iterations. New limit: " << current_match_limits[i] << std::endl;

            continue;
        }

        for (const auto &match : rewrite_matches)
        {
            matches.push_back(match);
        }
        rewrite_application_counts[i] += total_valid_matches;
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
    }

    return changed;
}

bool Rewriter::apply_rewrites(int max_iterations)
{
    bool any_changed = false;

    for (int i = 0; i < max_iterations; ++i)
    {
        if (!apply_one_iteration())
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
    while (apply_one_iteration())
    {
        changed = true;
    }
    return changed;
}
