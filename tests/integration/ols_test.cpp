#include "e_graph.h"
#include "expression.h"
#include "extractor.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <chrono>
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
    Rewriter kernel_rewriter(egraph, build_rewrite_sets({"lowering"}), 500);
    kernel_rewriter.apply_rewrites();
    Pruner::prune_symbolic_when_kernel_available(egraph);
    std::cout << "Doing extraction, num of nodes: " << egraph.num_nodes() << std::endl;
    Extractor extractor(egraph, cost_storage, true, 20);

    auto result = extractor.extract(id, 5);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_string(false) << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
    // ASSERT_TRUE(result[0].expr.to_string(true) == "Trsm(R(X), Gemv(Q(X)ᵀ, y, Zero_2x1))");
    // ASSERT_TRUE(result[0].expr.to_string(true) == "Sol(R(X), Q(X)ᵀ * y)");
}

TEST(Integration, OLSSymbolic) {
    auto start_total = std::chrono::high_resolution_clock::now();

    EGraph egraph(get_property_table());
    Expression root_expr("Inv(Tr(M) * M) * Tr(M) * n");

    auto start_add = std::chrono::high_resolution_clock::now();
    auto id = egraph.add_expression(root_expr);
    auto end_add = std::chrono::high_resolution_clock::now();

    auto start_rules = std::chrono::high_resolution_clock::now();
    std::vector<Rewrite> rules = build_rewrite_sets({"simplification", "transformation", "expansion"});
    auto end_rules = std::chrono::high_resolution_clock::now();

    Rewriter rewriter(egraph, rules, 1000, true);
    auto start_rewrite = std::chrono::high_resolution_clock::now();
    rewriter.apply_rewrites(10);
    auto end_rewrite = std::chrono::high_resolution_clock::now();

    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage, true, 20);

    auto start_extract = std::chrono::high_resolution_clock::now();
    auto results = extractor.extract(id, 5, {{"A", 30}, {"B", 10}});
    auto end_extract = std::chrono::high_resolution_clock::now();

    auto end_total = std::chrono::high_resolution_clock::now();

    std::cout << "\n--- OLSSymbolic Runtime Statistics ---" << std::endl;
    std::cout << "Add Expression: " << std::chrono::duration<double, std::milli>(end_add - start_add).count() << " ms"
              << std::endl;
    std::cout << "Build Rules:    " << std::chrono::duration<double, std::milli>(end_rules - start_rules).count()
              << " ms" << std::endl;
    std::cout << "Apply Rewrites: " << std::chrono::duration<double, std::milli>(end_rewrite - start_rewrite).count()
              << " ms" << std::endl;
    std::cout << "Extraction:     " << std::chrono::duration<double, std::milli>(end_extract - start_extract).count()
              << " ms" << std::endl;
    std::cout << "Total Time:     " << std::chrono::duration<double, std::milli>(end_total - start_total).count()
              << " ms" << std::endl;
    std::cout << "--------------------------------------\n" << std::endl;

    for (const auto &candidate : results) {
        std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
    ASSERT_TRUE(results[0].expr.to_string(true) == "Sol(R(M), Q(M)ᵀ * n)");
}
