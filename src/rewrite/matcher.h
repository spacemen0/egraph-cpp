#pragma once

#include "pattern.h"
#include <vector>

namespace egraph {
class EGraph;
class ENode;

class Matcher {
  public:
    explicit Matcher(EGraph &egraph);

    std::vector<Substitution> find_matches_in_eclass(Id eclass_id, const Pattern &pattern) const;

  private:
    EGraph &egraph;

    bool atoms_match(const Atom &pat_atom, const Atom &enode_atom) const;
    std::vector<Substitution>
    search_eclass_for_pattern(Id eclass_id, const Pattern &pattern, const Substitution &initial_subst) const;
};

} // namespace egraph
