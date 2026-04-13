#include "cost_storage.h"
#include "e_graph.h"
#include "extractor.h"
#include "pruner.h"
#include "rewrite_rules.h"
#include "rewriter.h"
#include "test_helpers.h"
#include "utils.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

TEST(Integration, MatrixPartialSet) {
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Inv(Mul(A, Z)), A), Z), X)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, mul_identity_right};

    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id);
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "X");
}

TEST(Integration, SimplifyComplexMatrixChain) {
    EGraph egraph(get_property_table());

    Expression root_expr("Mul(Tr(Mul(v, M)), Inv(Tr(v)))");
    Id root_id = egraph.add_expression(root_expr);
    std::cout << "Initial EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;

    std::vector<Rewrite> rules = {
        mul_identity_left,
        mat_transpose_prod,
        invert_cancel_right,
        mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(8);

    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    auto results = extractor.extract_symbolic(root_id);
    for (const auto &result : results) {
        std::cout << "Extracted expression: " << result.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << result.cost << std::endl;
    }
}

TEST(Integration, MinimalRealisticExplosionRules) {
    EGraph egraph(get_property_table());
    const auto root_id = egraph.add_expression(Expression("Mul(Mul(Inv(A), A), A)"));
    auto mul_assoc_right =
        make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))", nullptr, nullptr, 10);
    std::vector<Rewrite> rules = {
        mul_assoc_right,
        invert_cancel_left,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 2000, false, true);
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    Pruner pruner(egraph, extractor);

    constexpr int outer_iterations = 8;
    constexpr int rewrite_steps_per_iteration = 6;
    constexpr size_t prune_samples_per_iteration = 20;
    const std::vector<std::string> size_keys = {"A", "B"};

    std::cout << "Initial nodes: " << egraph.num_nodes() << std::endl;
    for (int iteration = 0; iteration < outer_iterations; ++iteration) {
        const bool changed = rewriter.apply_rewrites(rewrite_steps_per_iteration);
        std::cout << "Iteration " << (iteration + 1) << ": rewrites=" << (changed ? "changed" : "stalled")
                  << ", nodes before pruning=" << egraph.num_nodes() << std::endl;

        const auto bindings = sample_size_bindings(prune_samples_per_iteration, 1, 1000, size_keys);
        const auto prune_result = pruner.run({root_id}, bindings);
        std::cout << "Iteration " << (iteration + 1) << ": pruned=" << prune_result.nodes_pruned
                  << ", nodes after pruning=" << prune_result.nodes_after << std::endl;
    }

    auto result = extractor.extract(root_id);
    std::cout << "Num nodes after iterative rewriting/pruning: " << egraph.num_nodes() << std::endl;
    std::cout << "Final extracted expression: " << result.expr.to_human_string() << std::endl;
    std::cout << "Final extracted cost: " << result.cost << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, CyclicTermsThatDoNotExplode) {
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(A, Mul(Inv(A), A))"));
    std::vector<Rewrite> rules = {
        mul_assoc_left,
        invert_cancel_right,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 200);
    int i = 20;
    while (i-- > 0) {
        if (!rewriter.apply_one_iteration()) {
            break;
        }
        // egraph.to_img("cyclic_" + std::to_string(20 - i), "svg");
    }
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, MatrixChainSymbolicSizes) {
    EGraph egraph(get_property_table_with_symbolic_shapes());

    const Expression root_expr("Mul(Mul(Mul(Mul(Mul(Mul(A, B), C), D), E), F), G)");
    const Id root_id = egraph.add_expression(root_expr);
    Rewriter rewriter(egraph, {mul_assoc_left, mul_assoc_right}, 1000);
    rewriter.apply_rewrites();

    egraph.to_img("MatrixChain", "svg");
    CostStorage cost_storage(egraph);
    Extractor extractor(egraph, cost_storage);

    const auto symbolic_results = extractor.extract_symbolic(root_id);
    std::vector<Expression> candidate_expressions;
    candidate_expressions.reserve(symbolic_results.size());
    std::ranges::transform(symbolic_results, std::back_inserter(candidate_expressions), &ExtractionResult::expr);

    ASSERT_FALSE(candidate_expressions.empty());

    // for (auto expression : candidate_expressions) {
    //     std::cout << "Candidate expression: " << expression.to_human_string() << std::endl;
    // }

    std::vector<bool> expression_seen(candidate_expressions.size(), false);
    std::vector<std::string> size_keys = {"a", "b", "c", "d", "e", "f", "g", "h"};

    int k = 1000;
    for (int i = 0; i < k; ++i) {
        const auto extracted_expr = extractor.extract(root_id, sample_size_bindings(1, 100000, size_keys)).expr;
        const auto it = std::ranges::find(candidate_expressions, extracted_expr);
        if (it != candidate_expressions.end()) {
            const size_t index = static_cast<size_t>(std::distance(candidate_expressions.begin(), it));
            // if (!expression_seen[index]) {
            //     std::cout << "Expression " << index << " Matched at Sample " << (i + 1) << std::endl;
            //     std::cout << "Symbolic Cost: " << symbolic_results[index].cost << std::endl;
            //     std::cout << "Extracted Expression: " << extracted_expr.to_human_string() << std::endl;
            // }
            expression_seen[index] = true;
        }
    }

    const auto matched_count = std::ranges::count(expression_seen, true);
    std::cout << "Matched " << matched_count << " out of " << candidate_expressions.size() << " possible expressions."
              << std::endl;
    SUCCEED();
}

TEST(Integration, PrunerKeepsRootExtractable) {
    EGraph egraph(get_property_table_with_symbolic_shapes());
    const Id root_id = egraph.add_expression(Expression("Mul(Mul(Mul(Mul(Mul(Mul(A, B), C), D), E), F), G)"));

    std::vector<std::string> size_keys = {"a", "b", "c", "d", "e", "f", "g", "h"};
    auto bindings = sample_size_bindings(50, 1, 100000, size_keys);

    Rewriter rewriter(egraph, {mul_assoc_left, mul_assoc_right}, 10000, false, false);
    rewriter.apply_rewrites(10);
    const size_t before = egraph.num_nodes();

    CostStorage storage(egraph);
    Extractor extractor(egraph, storage);
    Pruner pruner(egraph, extractor);
    PruneResult result = pruner.run({root_id}, bindings);
    const size_t after = egraph.num_nodes();

    auto extracted = extractor.extract(root_id, bindings.front());

    EXPECT_LT(after, before);
    EXPECT_GT(result.nodes_pruned, 0U);
    EXPECT_TRUE(std::holds_alternative<double>(extracted.cost));
    std::cout << "Pruning removed " << result.nodes_pruned << " nodes, leaving " << result.nodes_after
              << " nodes. Extracted expression cost: " << std::get<double>(extracted.cost) << std::endl;
}
