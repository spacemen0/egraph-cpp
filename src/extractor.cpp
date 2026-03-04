#include <limits>
#include <algorithm>
#include "errors.h"
#include "extractor.h"

Extractor::Extractor(EGraph &egraph) : egraph(egraph), cost_storage(egraph.get_cost_storage())
{
    cost_storage.compute();
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
    std::ranges::transform(best_node->get_children(), std::back_inserter(children),
                           [this](Id child_id)
                           {
                               return build_expression(child_id);
                           });
    return Expression(best_node->get_atom(), children);
}

ExtractionResult Extractor::extract(Id class_id) const
{
    return {cost_storage.eclass_cost(class_id), build_expression(class_id)};
}
