#include "evaluator.h"

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
    auto id = egraph.add_expression(
        inverse(transpose(Expression("J")) * Expression("J")) * transpose(Expression("J")) * Expression("k"));
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, 50000, true);
    constexpr int max_iterations = 10;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        if (!rewriter.apply_one_iteration()) {
            break;
        }
    }
    auto alternative_expression = Expression("Sol(Get(QR(J),1), Tr(Get(QR(J),0)) * k)");
    auto alternative_id = egraph.add_expression(alternative_expression);
    ASSERT_TRUE(egraph.find_class_id(alternative_id) == egraph.find_class_id(id));
    CostStorage cost_storage(egraph);
    egraph.to_img("OLS_numeric", "svg");
    Pruner::prune_symbolic_when_kernel_available(egraph);
    std::cout << "Doing extraction, num of nodes: " << egraph.num_nodes() << std::endl;
    Extractor extractor(egraph, cost_storage, true, 20);

    auto result = extractor.extract(id);
    std::cout << "Candidate expression: " << result.expr.to_string(false) << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    EXPECT_TRUE(

        result.expr.to_string(false) == "Trsm_LN(Get(Geqrf(J), 1), Ormqr_LT(Geqrf(J), k))");

    Evaluator evaluator(egraph, result, nullptr);
    const auto &out = evaluator.evaluate();
    std::cout << "EVALUATOR OLS NUMERIC OUTPUT (first 5): ";
    for (int i = 0; i < 5; ++i)
        std::cout << out[i] << " ";
    std::cout << std::endl;
}

TEST(Integration, OLSSymbolic) {
    auto start_total = std::chrono::high_resolution_clock::now();

    EGraph egraph(get_property_table());

    Expression root_expr("Inv(Tr(M) * M) * Tr(M) * n");

    auto start_add = std::chrono::high_resolution_clock::now();
    auto id = egraph.add_expression(root_expr);
    auto end_add = std::chrono::high_resolution_clock::now();

    auto start_rules = std::chrono::high_resolution_clock::now();
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    auto end_rules = std::chrono::high_resolution_clock::now();

    Rewriter rewriter(egraph, rules, 50000, true);
    auto start_rewrite = std::chrono::high_resolution_clock::now();
    rewriter.apply_rewrites(10);
    auto end_rewrite = std::chrono::high_resolution_clock::now();

    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage, true, 20);
    Rewriter kernel_rewriter(egraph, build_rewrite_sets({"lowering"}), 50000);
    kernel_rewriter.apply_rewrites();
    Pruner::prune_symbolic_when_kernel_available(egraph);
    auto start_extract = std::chrono::high_resolution_clock::now();
    auto results = extractor.extract(id, {{"A", 30}, {"B", 20}});
    std::cout << "EVALUATOR OLS SYMBOLIC OUTPUT: " << results.expr.to_string(true) << std::endl;
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

    // for (const auto &candidate : results) {
    //     std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
    //     std::cout << "Cost: " << candidate.cost << std::endl;
    // }
    std::cout << "ACTUAL RESULT: " << results.expr.to_string(false) << std::endl;
    std::cout << "ACTUAL RESULT: " << results.expr.to_string(false) << std::endl;
    EXPECT_TRUE(

        results.expr.to_string(false) == "Trsm_LN(Get(Geqrf(M), 1), Ormqr_LT(Geqrf(M), n))");
}
