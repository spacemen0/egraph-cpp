#include "e_graph.h"
#include "extractor.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <iostream>

TEST(Integration, OLSNumeric) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Inv(Tr(X) * X) * Tr(X) * y"));
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, 500, true);
    constexpr int max_iterations = 10;
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
    Extractor extractor(egraph, cost_storage, true, 20);

    auto result = extractor.extract(id, 5);
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
    Extractor extractor(egraph, cost_storage, true, 20);
    auto results = extractor.extract(id, 5, {{"A", 300}, {"B", 10}});
    for (const auto &candidate : results) {
        std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}
