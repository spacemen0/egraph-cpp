#pragma once

#include "pattern.h"
#include <set>
#include <vector>

class EGraph;
class ENode;
class CostStorage;

class Matcher
{
public:
    explicit Matcher(const EGraph &egraph, const CostStorage *cost_storage = nullptr)
        : egraph(egraph), cost_storage(cost_storage) {}

    std::set<Substitution> find_matches_in_eclass(Id eclass_id, const Pattern &pattern) const;

private:
    const EGraph &egraph;
    const CostStorage *cost_storage;

    bool atoms_match(const Atom &pat_atom, const Atom &enode_atom) const;
    std::vector<Substitution> search_eclass_for_pattern(Id eclass_id, const Pattern &pattern, const Substitution &initial_subst) const;
    std::vector<const ENode *> ordered_nodes(Id eclass_id) const;
};
