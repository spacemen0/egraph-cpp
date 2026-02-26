#pragma once
#include <string>
#include <vector>
#include <functional>
#include "pattern.h"
#include "e_graph.h"

struct Rewrite
{
    std::string name;
    Pattern lhs;
    Pattern rhs;
    std::function<bool(const EGraph &, const Substitution &)> condition = nullptr;
    std::function<Id(EGraph &, const Substitution &)> applier = nullptr;
    std::optional<size_t> application_limit = std::nullopt;
};

class Rewriter
{
public:
    Rewriter(EGraph &egraph, std::vector<Rewrite> rewrites, size_t max_nodes)
        : egraph(egraph), rewrites(std::move(rewrites)), max_nodes(max_nodes)
    {
        rewrite_application_counts.resize(rewrites.size(), 0);
    }
    bool apply_one_iteration();
    bool apply_rewrites(int max_iterations);
    bool apply_rewrites();

private:
    EGraph &egraph;
    std::vector<Rewrite> rewrites;
    std::vector<size_t> rewrite_application_counts;
    size_t max_nodes;
};
