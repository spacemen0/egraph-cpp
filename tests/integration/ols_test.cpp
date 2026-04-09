#include "e_graph.h"
#include "extractor.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <format>
#include <gtest/gtest.h>

TEST(Integration, OLSSymbolic) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    std::vector<Rewrite> rules = {
        qr_inner_invert,    mat_transpose_prod, invert_mat_prod, orthonormal_transpose,
        invert_cancel_left, mul_identity_right, mul_assoc_left,  mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
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
            auto result = extractor.extract(id, {{"A", 100}, {"B", 50}});
            std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
            std::cout << "Cost: " << result.cost << std::endl;
            egraph.to_img("OLS", "svg");
            auto symbolic_results = extractor.extract_symbolic(id);
            // for (const auto &result : symbolic_results) {
            //     std::cout << "Candidate expression: " << result.expr.to_human_string() << std::endl;
            //     std::cout << "Cost: " << result.cost << std::endl;
            // }
            return;
        }
    }
    FAIL() << "Did not find the QR-based solution within the nodes limit.";
}

TEST(Integration, OLSNumeric) {
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(X), X) ) , Tr(X) ), y)"));

    std::vector<Rewrite> rules = {
        qr_inner_invert,    mat_transpose_prod, invert_mat_prod, orthonormal_transpose,
        invert_cancel_left, mul_identity_right, mul_assoc_left,  mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(5);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id);
    std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}

TEST(Integration, OLSSymbolicRewritePruneConverges) {
    EGraph egraph(get_property_table());
    const Id root_id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    const auto factorization_rules = get_rewrite_set_by_name("factorization");
    const auto algebraic_rules = get_rewrite_set_by_name("algebraic");
    const auto inverse_rules = get_rewrite_set_by_name("inverse");
    const auto orthogonality_rules = get_rewrite_set_by_name("orthogonality");

    std::vector<Rewrite> rules;
    rules.reserve(
        factorization_rules.size() + algebraic_rules.size() + inverse_rules.size() + orthogonality_rules.size());
    rules.insert(rules.end(), factorization_rules.begin(), factorization_rules.end());
    rules.insert(rules.end(), algebraic_rules.begin(), algebraic_rules.end());
    rules.insert(rules.end(), inverse_rules.begin(), inverse_rules.end());
    rules.insert(rules.end(), orthogonality_rules.begin(), orthogonality_rules.end());

    constexpr int outer_iterations = 8;
    constexpr int rewrite_steps_per_iteration = 6;
    constexpr size_t prune_samples_per_iteration = 20;
    const std::vector<std::string> size_keys = {"A", "B"};

    std::vector<size_t> nodes_after_iteration;
    nodes_after_iteration.reserve(outer_iterations);

    size_t total_pruned = 0;
    Rewriter rewriter(egraph, rules, 1000);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    Pruner pruner(egraph, extractor);

    for (int i = 0; i < outer_iterations; ++i) {

        rewriter.apply_rewrites(rewrite_steps_per_iteration);

        egraph.to_img(std::format("iteration_{}_before_pruning", i + 1), "svg");
        const auto bindings = sample_size_bindings(prune_samples_per_iteration, 1, 1000, size_keys);
        const auto prune_result = pruner.run({root_id}, bindings);
        egraph.to_img(std::format("iteration_{}_after_pruning", i + 1), "svg");
        total_pruned += prune_result.nodes_pruned;

        nodes_after_iteration.push_back(egraph.num_nodes());
        std::cout << "Iteration " << (i + 1) << ": nodes after iteration=" << nodes_after_iteration.back()
                  << ", pruned=" << prune_result.nodes_pruned << std::endl;

        const size_t n = nodes_after_iteration.size();
    }

    auto result = extractor.extract_symbolic(root_id);
    for (const auto &candidate : result) {
        std::cout << "Candidate expression: " << candidate.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << candidate.cost << std::endl;
    }
}