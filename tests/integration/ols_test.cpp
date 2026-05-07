#include "e_graph.h"
#include "extractor.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>

TEST(Integration, OLSNumeric) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Inv(Tr(X) * X) * Tr(X) * y"));
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, 2000, true);
    constexpr int max_iterations = 30;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        if (!rewriter.apply_one_iteration()) {
            break;
        }
    }
    auto alternative_expression = Expression("Sol(Get(QR(X),1), Tr(Get(QR(X),0)) * y)");
    auto alternative_id = egraph.add_expression(alternative_expression);
    ASSERT_TRUE(egraph.find_class_id(alternative_id) == egraph.find_class_id(id));
    CostStorage cost_storage(egraph);
    egraph.to_img("OLS_numeric", "svg");

    std::cout << "Doing extraction, num of nodes: " << egraph.num_nodes() << std::endl;
    Extractor extractor(egraph, cost_storage, true, 40, 10000000);

    // Evaluate the cost of the alternative directly
    auto tree_ext = extractor.tree_extract(alternative_id);
    std::cout << "Alternative Tree Cost: " << tree_ext.cost << std::endl;

    Cost direct_cost = 0.0;
    if (std::holds_alternative<Op>(alternative_expression.atom) &&
        std::get<Op>(alternative_expression.atom) == Op::Sol) {
        direct_cost = ENode(egraph.at(alternative_id).get_children(), Op::Sol).compute_local_cost(egraph);
    }
    std::cout << "Alternative Root Local Cost: " << direct_cost << std::endl;

    auto result = extractor.extract(id, 50);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}

TEST(Integration, OLSSymbolic) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Inv(Tr(M) * M) * Tr(M) * n"));
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, 500, true);
    rewriter.apply_rewrites(10);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage, true, 40);
    auto result = extractor.extract_symbolic(id);
    auto alternative_expression = Expression("Sol(Get(QR(M),1), Tr(Get(QR(M),0)) * n)");
    auto cost_from_extraction = std::find_if(result.begin(), result.end(), [&](const ExtractionResult &res) {
        return res.expr.to_string() == alternative_expression.to_string();
    });
    std::cout << "Alternative expression: " << alternative_expression.to_string() << std::endl;
    if (cost_from_extraction != result.end()) {
        std::cout << "Cost from extraction: " << cost_from_extraction->cost << std::endl;
    } else {
        std::cout << "Alternative expression not found in extraction results." << std::endl;
    }
    // auto results = extractor.extract(id, 10, {{"A", 300}, {"B", 10}});
    // for (const auto &candidate : results) {
    //     std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
    //     std::cout << "Cost: " << candidate.cost << std::endl;
    // }
}
