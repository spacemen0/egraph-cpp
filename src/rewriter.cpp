#include "rewriter.h"
#include <iostream>

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

    // Store matches to apply them in batch: (class_id, rewrite_index, substitution)
    std::vector<Match> matches;

    // Search phase
    std::vector<Id> class_ids = egraph.get_all_class_ids();
    for (Id class_id : class_ids)
    {
        // Only check root classes
        if (egraph.find_class_id(class_id) != class_id)
            continue;

        for (size_t i = 0; i < rewrites.size(); ++i)
        {
            const auto &rewrite = rewrites[i];
            std::set<Substitution> substs;
            egraph.find_matches_in_eclass(class_id, rewrite.lhs, substs);

            for (const auto &subst : substs)
            {
                matches.emplace_back(class_id, i, subst);
            }
        }
    }

    // Apply phase
    for (const auto &match : matches)
    {
        const auto &rewrite = rewrites[match.rewrite_idx];

        if (rewrite.condition && !rewrite.condition(match.subst, egraph))
        {
            continue;
        }

        if (Id rhs_id = instantiate(egraph, rewrite.rhs, match.subst); egraph.union_classes(match.class_id, rhs_id))
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
