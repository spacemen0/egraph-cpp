#include "e_graph.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

TEST(Integration, OLSPruneConverges) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair("A", "B"), .flags = {.is_positive_definite = true, .is_tall = true}});
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});

    std::vector<size_t> nodes_after_iteration;

    size_t total_pruned = 0;
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 500, .enable_backoff = true}});
    Extractor extractor(egraph, EGraphConfig{.extractor = {.max_depth = 20}, .enable_logging = true});
    Pruner pruner(egraph, extractor);
    Id root_id = egraph.add_expression(Expression("(Inv(Tr(M) * M) * Tr(M)) * n"));

    PruneOptions options{
        .num_iterations = 8,
        .rewrite_steps_per_iteration = 20,
        .prune_samples_per_iteration = 20,
        .max_results_per_binding = 5,
        .size_keys = {"A", "B"},
    };

    auto start_time = std::chrono::high_resolution_clock::now();
    auto pre_iteration = [&](int i) {
        start_time = std::chrono::high_resolution_clock::now();
    };
    auto post_iteration = [&](int i, const PruneResult &prune_result) {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        total_pruned += prune_result.nodes_pruned;
        nodes_after_iteration.push_back(egraph.num_nodes());
        std::cout << "Iteration " << (i + 1) << ": nodes after iteration=" << nodes_after_iteration.back()
                  << ", pruned=" << prune_result.nodes_pruned << ", time=" << duration.count() << "s" << std::endl;
    };

    pruner.rewrite_and_prune({root_id}, rewriter, options, pre_iteration, post_iteration);
    Rewriter kernel_rewriter(egraph, build_rewrite_sets({"lowering"}), EGraphConfig{.rewrite = {.node_limit = 500}});
    kernel_rewriter.apply_rewrites();
    egraph.to_img("egraph_after_pruning", "svg");
    Pruner::prune_symbolic_when_kernel_available(egraph);
    egraph.to_img("egraph_after_symbolic_pruning", "svg");
    auto result = extractor.extract_symbolic(root_id);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
    EXPECT_TRUE(std::any_of(result.begin(), result.end(), [](const auto &c) {
        return c.expr.to_string(true) == "Trsm_LN(R(M), Ormqr_LT(Geqrf(M), n))";
    }));
}
