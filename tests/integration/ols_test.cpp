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
    std::vector<Rewrite> rules = build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality"});
    Rewriter rewriter(egraph, rules, 4000, true);
    auto should_be_root_id = egraph.add_expression(Expression("Mul(Inv(Get(QR(X), 1)), Mul(Tr(Get(QR(X), 0)), y))"));

    bool found_qr_form = false;
    constexpr int max_iterations = 20;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        rewriter.apply_one_iteration();
        if (egraph.find_class_id(should_be_root_id) == egraph.find_class_id(id)) {
            found_qr_form = true;
            std::cout << "Found the QR-based solution in iteration " << (iteration + 1)
                      << "! Num nodes: " << egraph.num_nodes() << std::endl;
        }
    }
    ASSERT_TRUE(found_qr_form) << "Did not find the QR-based numeric solution within iteration budget.";

    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id);
    std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}

TEST(Integration, OLSSymbolic) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair("A", "B"), .flags = {.is_positive_definite = true, .is_tall = true}});
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    std::vector<Rewrite> rules = build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality"});
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
            Extractor extractor(egraph, cost_storage);
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
    std::vector<Rewrite> rules = build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality"});

    constexpr int outer_iterations = 8;
    constexpr int rewrite_steps_per_iteration = 10;
    constexpr size_t prune_samples_per_iteration = 50;
    const std::vector<std::string> size_keys = {"A", "B"};

    std::vector<size_t> nodes_after_iteration;
    nodes_after_iteration.reserve(outer_iterations);

    size_t total_pruned = 0;
    Rewriter rewriter(egraph, rules, 1500, true);
    CostStorage cost_storage(egraph);
    Id root_id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    for (int i = 0; i < outer_iterations; ++i) {
        Extractor extractor(egraph, cost_storage);
        Pruner pruner(egraph, extractor);
        rewriter.apply_rewrites(rewrite_steps_per_iteration);
        auto should_be_root_id =
            egraph.add_expression(Expression("Mul(Inv(Get(QR(M), 1)), Mul(Tr(Get(QR(M), 0)), n))"));
        if (egraph.find_class_id(root_id) == egraph.find_class_id(should_be_root_id)) {
            std::cout << "Found the QR-based solution in iteration " << (i + 1) << "! Num nodes: " << egraph.num_nodes()
                      << std::endl;
        }
        // egraph.to_img(std::format("iteration_{}_before_pruning", i + 1), "svg");
        const auto bindings = sample_size_bindings(prune_samples_per_iteration, 1, 1000, size_keys);
        const auto prune_result = pruner.run({root_id}, bindings);
        egraph.to_img(std::format("iteration_{}_after_pruning", i + 1), "svg");
        total_pruned += prune_result.nodes_pruned;

        nodes_after_iteration.push_back(egraph.num_nodes());
        std::cout << "Iteration " << (i + 1) << ": nodes after iteration=" << nodes_after_iteration.back()
                  << ", pruned=" << prune_result.nodes_pruned << std::endl;

        const size_t n = nodes_after_iteration.size();
    }
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract_symbolic(root_id);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}