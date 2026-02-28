#include "matcher.h"

#include "cost_storage.h"
#include "e_graph.h"

#include <algorithm>

Matcher::Matcher(const EGraph &egraph)
    : egraph(egraph), cost_storage(egraph.get_cost_storage()) {}

bool Matcher::atoms_match(const Atom &pat_atom, const Atom &enode_atom) const
{
    if (pat_atom.index() != enode_atom.index())
        return false;

    if (const auto op1 = std::get_if<Op>(&pat_atom))
        return *op1 == std::get<Op>(enode_atom);

    if (const auto s1 = std::get_if<std::string>(&pat_atom))
        return *s1 == std::get<std::string>(enode_atom);

    if (const auto i1 = std::get_if<int>(&pat_atom))
        return *i1 == std::get<int>(enode_atom);

    return false;
}

std::vector<const ENode *> Matcher::ordered_nodes(Id eclass_id, size_t limit) const
{
    const auto &nodes = egraph.get_class_nodes(eclass_id);
    std::vector<const ENode *> ordered(nodes.begin(), nodes.end());

    std::ranges::sort(ordered, [&](const ENode *lhs, const ENode *rhs)
                      { return cost_storage.node_cost(*lhs) < cost_storage.node_cost(*rhs); });

    if (limit > 0 && limit < ordered.size())
    {
        ordered.resize(limit);
    }
    return ordered;
}

std::set<Substitution> Matcher::find_matches_in_eclass(Id eclass_id, const Pattern &pattern, size_t limit) const
{
    Substitution initial_subst;
    std::set<Substitution> out_substitutions;
    auto matches = search_eclass_for_pattern(eclass_id, pattern, initial_subst, limit);
    out_substitutions.insert(matches.begin(), matches.end());
    return out_substitutions;
}

std::vector<Substitution> Matcher::search_eclass_for_pattern(Id eclass_id, const Pattern &pattern, const Substitution &initial_subst, size_t limit) const
{
    std::vector<Substitution> results;
    Id canonical_id = egraph.find_class_id(eclass_id);

    if (const std::string *str_atom = std::get_if<std::string>(&pattern.atom))
    {
        if (str_atom->starts_with('?'))
        {
            auto var_name = str_atom->substr(1);
            auto it = initial_subst.find(var_name);

            if (it != initial_subst.end())
            {
                if (it->second == canonical_id)
                {
                    results.push_back(initial_subst);
                }
            }
            else
            {
                Substitution new_subst = initial_subst;
                new_subst[var_name] = canonical_id;
                results.push_back(new_subst);
            }
            return results;
        }
    }

    for (const ENode *node : ordered_nodes(canonical_id, limit))
    {
        if (!atoms_match(pattern.atom, node->get_atom()) || node->get_children().size() != pattern.children.size())
        {
            continue;
        }

        std::vector<Substitution> current_matches = {initial_subst};
        bool possible = true;

        for (size_t i = 0; i < pattern.children.size(); ++i)
        {
            std::vector<Substitution> next_matches;
            const Pattern &child_pattern = pattern.children[i];
            Id child_eclass_id = node->get_children()[i];

            for (const auto &subst : current_matches)
            {
                auto child_results = search_eclass_for_pattern(child_eclass_id, child_pattern, subst, limit);
                next_matches.insert(next_matches.end(), child_results.begin(), child_results.end());
            }

            current_matches = std::move(next_matches);
            if (current_matches.empty())
            {
                possible = false;
                break;
            }
        }

        if (possible)
        {
            results.insert(results.end(), current_matches.begin(), current_matches.end());
        }
    }

    return results;
}
