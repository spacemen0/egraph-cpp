#pragma once

#include "pattern.h"
#include <set>
#include <vector>

class EGraph;
class ENode;

class Matcher {
  public:
    explicit Matcher(EGraph &egraph);

    std::set<Substitution> find_matches_in_eclass(Id eclass_id, const Pattern &pattern, size_t limit = 0) const;

  private:
    EGraph &egraph;

    bool atoms_match(const Atom &pat_atom, const Atom &enode_atom) const;
    std::vector<Substitution> search_eclass_for_pattern(
        Id eclass_id, const Pattern &pattern, const Substitution &initial_subst, size_t limit = 0) const;
    std::vector<const ENode *> ordered_nodes(Id eclass_id, size_t limit = 0) const;
};
