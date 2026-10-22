#pragma once

#include "basic_types.h"
#include <string>
#include <vector>

namespace egraph {
struct ParsedAtom {
    Atom atom;
    std::vector<std::string> children_strings;
};

using Cost = double;

struct PruneResult {
    size_t nodes_before = 0;
    size_t nodes_after = 0;
    size_t nodes_pruned = 0;
    size_t classes_with_removed_nodes = 0;
    bool changed = false;
};
} // namespace egraph
