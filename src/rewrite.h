#pragma once
#include <string>
#include <vector>
#include "pattern.h"
#include "e_graph.h"

struct Rewrite
{
    std::string name;
    Pattern lhs;
    Pattern rhs;
};

struct Match
{
    Id class_id;
    size_t rewrite_idx;
    Substitution subst;
};

/// @brief Applies all rewrites to the e-graph until saturation or a limit.
/// @return true if any rewrite was applied
bool apply_rewrites(EGraph &egraph, const std::vector<Rewrite> &rewrites, int max_iterations = 30);
