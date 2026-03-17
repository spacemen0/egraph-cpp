#include <limits>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include "errors.h"
#include "extractor.h"

Extractor::Extractor(EGraph &egraph) : egraph(egraph), cost_storage(egraph.get_cost_storage())
{
}

std::optional<Extractor::NumericSearchResult> Extractor::find_best_numeric_dag(Id root_class_id, const SizeBindings *size_bindings) const
{
    Id root = egraph.find_class_id(root_class_id);
    if (!size_bindings)
    {
        if (auto cached = cost_storage.cached_root_extraction(root); cached.has_value())
        {
            return NumericSearchResult{cached->cost, cached->choices};
        }
    }

    std::vector<PendingClass> pending = {{root, {}}};
    std::unordered_map<Id, const ENode *> current_choices;
    std::unordered_map<Id, const ENode *> best_choices;
    double best_cost = std::numeric_limits<double>::infinity();

    search_best_numeric_dag(pending, current_choices, size_bindings, 0.0, best_cost, best_choices);
    if (!std::isfinite(best_cost))
    {
        return std::nullopt;
    }
    if (!size_bindings)
    {
        cost_storage.store_root_extraction(root, best_cost, best_choices);
    }
    return NumericSearchResult{best_cost, std::move(best_choices)};
}

std::vector<Extractor::SymbolicSearchResult> Extractor::find_symbolic_dags(Id root_class_id) const
{
    Id root = egraph.find_class_id(root_class_id);

    std::vector<PendingClass> pending = {{root, {}}};
    std::unordered_map<Id, const ENode *> current_choices;
    std::vector<SymbolicSearchResult> results;
    SymbolicCost initial_cost;

    search_symbolic_dags(pending, current_choices, initial_cost, results);
    return results;
}

/// @param pending Stack of e-classes that still need a node choice, each annotated with the ancestors on the path that reached it.
/// @param current_choices Map from e-class ID to the chosen ENode for the current search path.
/// @param current_cost Accumulated cost of the current partial DAG (sum of local costs of chosen nodes).
/// @param best_cost The cost of the best complete solution found so far.
/// @param best_choices The node choices of the best solution found so far.
void Extractor::search_best_numeric_dag(
    const std::vector<PendingClass> &pending,
    std::unordered_map<Id, const ENode *> &current_choices,
    const SizeBindings *size_bindings,
    double current_cost,
    double &best_cost,
    std::unordered_map<Id, const ENode *> &best_choices) const
{
    if (pending.empty())
    {
        if (current_cost < best_cost)
        {
            best_cost = current_cost;
            best_choices = current_choices;
        }
        return;
    }

    std::vector<PendingClass> local_pending = pending;
    PendingClass current_pending = local_pending.back();
    Id current = egraph.find_class_id(current_pending.class_id);
    local_pending.pop_back();

    for (const ENode *candidate : egraph.get_class_nodes(current))
    {
        Cost local_cost = candidate->compute_local_cost(egraph, size_bindings);

        // Skip symbolic-cost candidates.
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
        std::vector<PendingClass> child_pending = local_pending;
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

            bool child_already_pending = std::ranges::any_of(
                child_pending,
                [&](const PendingClass &entry)
                {
                    return egraph.find_class_id(entry.class_id) == child_root;
                });

            if (!current_choices.contains(child_root) && !child_already_pending) // only calculate unvisited children
            {
                child_pending.emplace_back(PendingClass{child_root, child_ancestors});
            }
        }

        if (!has_cycle)
        {
            // Recursively search with this candidate chosen.
            search_best_numeric_dag(child_pending, current_choices, size_bindings, next_cost, best_cost, best_choices);
        }

        // Return to the previous state.
        current_choices.erase(current);
    }
}

void Extractor::search_symbolic_dags(const std::vector<PendingClass> &pending, std::unordered_map<Id, const ENode *> &current_choices, SymbolicCost current_cost, std::vector<SymbolicSearchResult> &results) const
{
    if (pending.empty())
    {
        results.push_back(SymbolicSearchResult{current_cost, current_choices});
        return;
    }

    std::vector<PendingClass> local_pending = pending;
    PendingClass current_pending = local_pending.back();
    Id current = egraph.find_class_id(current_pending.class_id);
    local_pending.pop_back();

    for (const ENode *candidate : egraph.get_class_nodes(current))
    {
        Cost local_cost = candidate->compute_local_cost(egraph);

        SymbolicCost next_cost = current_cost;
        if (std::holds_alternative<SymbolicCost>(local_cost))
        {
            next_cost = next_cost + std::get<SymbolicCost>(local_cost);
        }
        else if (std::holds_alternative<double>(local_cost) && std::get<double>(local_cost) != 0.0)
        {
            continue;
        }

        current_choices[current] = candidate;

        // Add children e-classes to the pending list. A child that appears in the
        // ancestor set for this dependency path would form a cycle.
        std::vector<PendingClass> child_pending = local_pending;
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

            bool child_already_pending = std::ranges::any_of(
                child_pending,
                [&](const PendingClass &entry)
                {
                    return egraph.find_class_id(entry.class_id) == child_root;
                });

            if (!current_choices.contains(child_root) && !child_already_pending) // only calculate unvisited children
            {
                child_pending.emplace_back(PendingClass{child_root, child_ancestors});
            }
        }

        if (!has_cycle)
        {
            // Recursively search with this candidate chosen.
            search_symbolic_dags(child_pending, current_choices, next_cost, results);
        }

        // Return to the previous state.
        current_choices.erase(current);
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

ExtractionResult Extractor::extract(Id class_id) const
{
    if (auto best = find_best_numeric_dag(class_id); best.has_value())
    {
        std::unordered_set<Id> visiting;
        return {best->cost, build_expression(class_id, best->choices, visiting)};
    }

    throw std::runtime_error("Runtime error: no numeric DAG found for root class");
}

ExtractionResult Extractor::extract(Id class_id, const SizeBindings &size_bindings) const
{
    if (auto best = find_best_numeric_dag(class_id, &size_bindings); best.has_value())
    {
        std::unordered_set<Id> visiting;
        return {best->cost, build_expression(class_id, best->choices, visiting)};
    }

    throw std::runtime_error("Runtime error: no numeric DAG found for root class under supplied size bindings");
}

std::vector<ExtractionResult> Extractor::extract_symbolic(Id class_id) const
{
    auto symbolic_dags = find_symbolic_dags(class_id);
    std::vector<ExtractionResult> results;
    for (const auto &dag : symbolic_dags)
    {
        std::unordered_set<Id> visiting;
        results.push_back({dag.cost, build_expression(class_id, dag.choices, visiting)});
    }
    return results;
}
