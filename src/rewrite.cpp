#include "rewrite.h"
#include <iostream>

// Instantiate a pattern into the EGraph
static Id instantiate(EGraph &egraph, const Pattern &pattern, const Substitution &subst)
{
    if (std::holds_alternative<PatternVar>(pattern.atom))
    {
        const auto &var = std::get<PatternVar>(pattern.atom);
        return subst.at(var.name);
    }
    else
    {
        const auto &op = std::get<Op>(pattern.atom);
        Children children;
        children.reserve(pattern.children.size());
        for (const auto &child_pat : pattern.children)
        {
            children.emplace_back(instantiate(egraph, child_pat, subst));
        }
        ENode node(children, Atom(op));
        return egraph.add_node(node); // new id or existing id
    }
}

bool apply_one_iteration(EGraph &egraph, const std::vector<Rewrite> &rewrites)
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

        Id rhs_id = instantiate(egraph, rewrite.rhs, match.subst);

        if (egraph.union_classes(match.class_id, rhs_id))
        {
            changed = true;
        }
    }

    if (changed)
    {
        egraph.rebuild();
    }

    return changed;
}

bool apply_rewrites(EGraph &egraph, const std::vector<Rewrite> &rewrites, int max_iterations)
{
    bool any_changed = false;

    for (int i = 0; i < max_iterations; ++i)
    {
        if (!apply_one_iteration(egraph, rewrites))
        {
            break;
        }
        any_changed = true;
    }

    return any_changed;
}
