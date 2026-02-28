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
    size_t initial_match_limit = std::numeric_limits<size_t>::max();
};

class Rewriter
{
public:
    Rewriter(EGraph &egraph, std::vector<Rewrite> rewrites, size_t max_nodes, bool enable_backoff = false)
        : egraph(egraph), rewrites(std::move(rewrites)), max_nodes(max_nodes), enable_backoff(enable_backoff)
    {
        current_match_limits.resize(this->rewrites.size());
        rewrite_application_counts.resize(this->rewrites.size(), 0);
        ban_iterations_remaining.resize(this->rewrites.size(), 0);
        ban_duration_next.resize(this->rewrites.size(), 1);

        for (size_t i = 0; i < this->rewrites.size(); ++i)
        {
            current_match_limits[i] = this->rewrites[i].initial_match_limit;
        }
    }
    bool apply_one_iteration();
    bool apply_rewrites(int max_iterations);
    bool apply_rewrites();

private:
    EGraph &egraph;
    bool enable_backoff;
    std::vector<Rewrite> rewrites;
    std::vector<size_t> current_match_limits;       // Current limit (doubles when banning)
    std::vector<size_t> rewrite_application_counts; // Accumulated applications in current iteration
    std::vector<size_t> ban_iterations_remaining;   // How many iterations left in current ban
    std::vector<size_t> ban_duration_next;          // Duration for next ban (starts at 1, doubles)
    size_t max_nodes;
};
