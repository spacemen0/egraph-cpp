#include "e_graph.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>
#include <iostream>

TEST(Integration, OLSNumeric) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(X), X) ) , Tr(X) ), y)"));
    std::vector<Rewrite> rules =
        build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality", "zero-negation", "solver"});
    Rewriter rewriter(egraph, rules, 700, true);

    bool found_qr_form = false;
    constexpr int max_iterations = 20;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        rewriter.apply_one_iteration();
        // if (egraph.find_class_id(should_be_root_id) == egraph.find_class_id(id)) {
        //     found_qr_form = true;
        //     std::cout << "Found the QR-based solution in iteration " << (iteration + 1)
        //               << "! Num nodes: " << egraph.num_nodes() << std::endl;
        // }
    }
    auto alternative_expression = Expression("Sol(Mul(Tr(X), X), Mul(Tr(X), y))");
    auto alternative_id = egraph.add_expression(alternative_expression);
    ASSERT_TRUE(egraph.find_class_id(alternative_id) == egraph.find_class_id(id));
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage, true);
    auto result = extractor.extract(id, 1);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}

TEST(Integration, OLSSymbolic) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair("A", "B"), .flags = {.is_positive_definite = true, .is_tall = true}});
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, 10000, true);
    int iteration = 0;
    while (rewriter.apply_one_iteration()) {
        iteration++;
        auto should_be_root_id =
            egraph.add_expression(Expression("Mul(Inv(Get(QR(M), 1)), Mul(Tr(Get(QR(M), 0)), n))"));
        if (egraph.find_class_id(id) == egraph.find_class_id(should_be_root_id)) {
            std::cout << "Found the QR-based solution in iteration " << iteration
                      << "! Num nodes: " << egraph.num_nodes() << std::endl;
            std::cout << "Sample matrix sizes to try out extraction..." << std::endl;
            CostStorage cost_storage(egraph);
            Extractor extractor(egraph, cost_storage, true);
            auto result = extractor.extract(id, {{"A", 3}, {"B", 2}});
            std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
            std::cout << "Cost: " << result.cost << std::endl;
            // egraph.to_img("OLS", "svg");
            // auto symbolic_results = extractor.extract_symbolic(id);
            // for (const auto &result : symbolic_results) {
            //     std::cout << "Candidate expression: " << result.expr.to_human_string() << std::endl;
            //     std::cout << "Cost: " << result.cost << std::endl;
            // }
            return;
        }
    }
    FAIL() << "Did not find the QR-based solution within the nodes limit.";
}

TEST(Integration, OLSSymbolicRewritePruneConverges) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair("A", "B"), .flags = {.is_positive_definite = true, .is_tall = true}});
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});

    std::vector<size_t> nodes_after_iteration;

    size_t total_pruned = 0;
    Rewriter rewriter(egraph, rules, 1000, true);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    Pruner pruner(egraph, extractor);
    Id root_id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    PruneOptions options{
        .num_iterations = 8,
        .rewrite_steps_per_iteration = 10,
        .prune_samples_per_iteration = 5,
        .max_results_per_binding = 3,
        .size_keys = {"A", "B"},
    };

    auto callback = [&](int i, const PruneResult &prune_result) {
        auto should_be_root_id =
            egraph.add_expression(Expression("Mul(Inv(Get(QR(M), 1)), Mul(Tr(Get(QR(M), 0)), n))"));
        total_pruned += prune_result.nodes_pruned;
        nodes_after_iteration.push_back(egraph.num_nodes());
        std::cout << "Iteration " << (i + 1) << ": nodes after iteration=" << nodes_after_iteration.back()
                  << ", pruned=" << prune_result.nodes_pruned << std::endl;
    };

    pruner.rewrite_and_run({root_id}, rewriter, options, callback);

    auto result = extractor.extract_symbolic(root_id);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}