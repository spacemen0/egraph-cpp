#include "extractor.h"
#include <limits>
#include "errors.h"

Extractor::Extractor(EGraph &egraph) : egraph(egraph), cost_storage(egraph.get_cost_storage())
{
    cost_storage.recompute();
}

Expression Extractor::build_expression(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    const ENode *best_node = cost_storage.best_node(root);
    if (!best_node)
    {
        throw std::runtime_error("Runtime error: !best_node");
    }

    std::vector<Expression> children;
    for (Id child : best_node->get_children())
    {
        children.push_back(build_expression(child));
    }
    return Expression(best_node->get_atom(), children);
}

ExtractionResult Extractor::extract(Id class_id) const
{
    Id root = egraph.find_class_id(class_id);
    if (!cost_storage.has_finite_cost(root))
    {
        throw std::runtime_error("Runtime error: !best_node");
    }
    return {cost_storage.eclass_cost(root), build_expression(root)};
}
