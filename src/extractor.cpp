#include <limits>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include "errors.h"
#include "extractor.h"

Extractor::Extractor(EGraph &egraph) : egraph(egraph), cost_storage(egraph.get_cost_storage())
{
    cost_storage.compute();
}

std::optional<Extractor::SearchResult> Extractor::find_best_numeric_dag(Id root_class_id) const
{
    Id root = egraph.find_class_id(root_class_id);
    if (auto cached = cost_storage.cached_root_extraction(root); cached.has_value())
    {
        return SearchResult{cached->cost, cached->choices};
    }

    std::vector<PendingClass> pending = {{root, {}}};
    std::unordered_map<Id, const ENode *> current_choices;
    std::unordered_map<Id, const ENode *> best_choices;
    double best_cost = std::numeric_limits<double>::infinity();

    search_best_numeric_dag(pending, current_choices, 0.0, best_cost, best_choices);
    if (!std::isfinite(best_cost))
    {
        return std::nullopt;
    }
    cost_storage.store_root_extraction(root, best_cost, best_choices);
    return SearchResult{best_cost, std::move(best_choices)};
}

/// @brief Exhaustive branch-and-bound search to find the optimal DAG extraction for a given root e-class.
///
/// This function explores all possible combinations of node choices across e-classes to find the
/// minimum-cost DAG.
///
/// @param pending Stack of e-classes that still need a node choice, each annotated with the dependency ancestors on the path that reached it.
/// @param current_choices Map from e-class ID to the chosen ENode for the current search path (modified during search).
/// @param current_cost Accumulated cost of the current partial DAG (sum of local costs of chosen nodes).
/// @param best_cost (in/out) The cost of the best complete solution found so far. Updated when a better solution is found.
/// @param best_choices (out) Map storing the node choices of the best solution found so far.
void Extractor::search_best_numeric_dag(
    std::vector<PendingClass> &pending,
    std::unordered_map<Id, const ENode *> &current_choices,
    double current_cost,
    double &best_cost,
    std::unordered_map<Id, const ENode *> &best_choices) const
{
    // Process e-classes in the pending stack.
    while (!pending.empty())
    {
        PendingClass current_pending = pending.back();
        Id current = egraph.find_class_id(current_pending.class_id);

        // Skip e-classes that already have a chosen node
        if (current_choices.contains(current))
        {
            pending.pop_back();
            continue;
        }

        // Found an e-class that needs a node choice. Save the rest of the pending stack for backtracking after trying candidates for this e-class.
        std::vector<PendingClass> parent_pending = pending;
        parent_pending.pop_back();

        for (const ENode *candidate : egraph.get_class_nodes(current))
        {
            Cost local_cost = candidate->compute_local_cost(egraph);

            // Skip symbolic-cost candidates
            if (!std::holds_alternative<double>(local_cost))
            {
                continue;
            }

            double next_cost = current_cost + std::get<double>(local_cost);
            if (next_cost >= best_cost)
            {
                continue;
            }

            current_choices[current] = candidate;

            // Add children e-classes to the pending list. A child that appears in the
            // ancestor set for this dependency path would form a cycle.
            std::vector<PendingClass> child_pending = parent_pending;
            std::unordered_set<Id> child_ancestors = current_pending.ancestors;
            child_ancestors.insert(current);
            bool has_cycle = false;
            for (Id child : candidate->get_children())
            {
                Id child_root = egraph.find_class_id(child);
                if (child_ancestors.contains(child_root))
                {
                    has_cycle = true;
                    break;
                }
                if (!current_choices.contains(child_root)) // only calculate unvisited children
                {
                    child_pending.push_back({child_root, child_ancestors});
                }
            }

            if (!has_cycle)
            {
                // Recursively search with this candidate chosen.
                search_best_numeric_dag(child_pending, current_choices, next_cost, best_cost, best_choices);
            }

            // Return to the previous state
            current_choices.erase(current);
        }

        // After trying all candidates for this one e-class, backtrack to parent level
        return;
    }

    if (current_cost < best_cost)
    {
        best_cost = current_cost;
        best_choices = current_choices;
    }
}

Expression Extractor::build_expression(
    Id class_id,
    const std::unordered_map<Id, const ENode *> &choices,
    std::unordered_set<Id> &visiting) const
{
    Id root = egraph.find_class_id(class_id);
    auto it = choices.find(root);
    if (it == choices.end())
    {
        throw std::runtime_error("Runtime error: missing choice for reachable e-class");
    }

    if (visiting.contains(root))
    {
        throw std::runtime_error("Runtime error: cycle detected while building expression");
    }
    visiting.insert(root);

    std::vector<Expression> children;
    children.reserve(it->second->get_children().size());
    for (Id child_id : it->second->get_children())
    {
        children.push_back(build_expression(child_id, choices, visiting));
    }
    visiting.erase(root);
    return Expression(it->second->get_atom(), children);
}

Expression Extractor::build_expression_from_cost_storage(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    const ENode *best_node = cost_storage.best_node(root);
    if (!best_node)
    {
        throw std::runtime_error("Runtime error: !best_node");
    }

    std::vector<Expression> children;
    children.reserve(best_node->get_children().size());
    std::ranges::transform(best_node->get_children(), std::back_inserter(children),
                           [this](Id child_id)
                           {
                               return build_expression_from_cost_storage(child_id);
                           });
    return Expression(best_node->get_atom(), children);
}

ExtractionResult Extractor::extract(Id class_id) const
{
    if (auto best = find_best_numeric_dag(class_id); best.has_value())
    {
        std::unordered_set<Id> visiting;
        return {best->cost, build_expression(class_id, best->choices, visiting)};
    }

    // Fallback to global cost storage for non-numeric/symbolic-only cases.
    return {cost_storage.eclass_cost(class_id), build_expression_from_cost_storage(class_id)};
}
