#pragma once

#include "basic_types.h"
#include <vector>

namespace egraph {
class UnionFind {
  public:
    UnionFind() = default;

    Id find_root(Id current) const;

    Id find_and_compress(Id current);

    void unite(Id x, Id y);
    std::vector<Id> get_parents() const noexcept;
    Id make_set();

  private:
    std::vector<Id> parents;
};

} // namespace egraph
