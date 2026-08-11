#include "basic_types.h"
#include "e_graph.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include "transformation.h"
#include "utils.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

TEST(Integration, MatrixPartialSet) {
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Inv(A * Z) * A * Z * X"));
    std::vector<Rewrite> rules = {mul_assoc, invert_cancel_left, mul_identity_right};

    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000}});
    rewriter.apply_rewrites(4);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<uint32_t>(result.expr.atom));
    EXPECT_EQ(std::get<uint32_t>(result.expr.atom), register_string_in_lookup("X"));
}

TEST(Integration, SimplifyComplexMatrixChain) {
    EGraph egraph(get_property_table());

    Expression root_expr("Tr(v * M) * Inv(Tr(v))");
    Id root_id = egraph.add_expression(root_expr);
    std::cout << "Initial EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;

    std::vector<Rewrite> rules = {
        mul_identity_left,
        mat_transpose_prod,
        invert_cancel_right,
        mul_assoc,
    };
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000}});
    rewriter.apply_rewrites(8);

    Extractor extractor(egraph);
    auto results = extractor.extract_symbolic(root_id);
    for (const auto &result : results) {
        std::cout << "Extracted expression: " << result.expr.to_string() << std::endl;
        std::cout << "Cost: " << result.cost << std::endl;
    }
}

TEST(Integration, MinimalRealisticExplosionRules) {
    EGraph egraph(get_property_table());
    const auto root_id = egraph.add_expression(Expression("Inv(A) * A * A"));
    auto mul_assoc_right =
        make_rewrite("mul-assoc-right", "?a * ?b * ?c", "?a * (?b * ?c)", false, nullptr, nullptr, 10);
    std::vector<Rewrite> rules = {
        mul_assoc_right,
        invert_cancel_left,
        mul_identity_right,
    };
    Rewriter rewriter(
        egraph, rules,
        EGraphConfig{.rewrite = {.node_limit = 2000, .enable_backoff = false, .enable_node_match_limit = true}});
    Extractor extractor(egraph);
    Pruner pruner(egraph, extractor);

    constexpr int outer_iterations = 8;
    constexpr int rewrite_steps_per_iteration = 6;
    constexpr size_t prune_samples_per_iteration = 20;

    for (int iter = 0; iter < outer_iterations; ++iter) {
        bool changed = rewriter.apply_rewrites(rewrite_steps_per_iteration);

        std::cout << "Iteration " << iter + 1 << ": " << (changed ? "rewrites=changed" : "rewrites=stalled")
                  << ", nodes before pruning=" << egraph.num_nodes() << std::endl;

        const std::vector<std::string> size_keys = {"A", "B"};
        const auto bindings = sample_size_bindings(prune_samples_per_iteration, 1, 1000, size_keys);
        const auto prune_result = pruner.prune({root_id}, bindings, 1);

        std::cout << "Iteration " << iter + 1 << ": pruned=" << prune_result.nodes_pruned
                  << ", nodes after pruning=" << egraph.num_nodes() << std::endl;

        if (!changed && prune_result.nodes_pruned == 0) {
            break;
        }
    }

    std::cout << "Num nodes after iterative rewriting/pruning: " << egraph.num_nodes() << std::endl;

    auto result = extractor.extract(root_id);
    std::cout << "Final extracted expression: " << result.expr.to_string() << std::endl;
    std::cout << "Final extracted cost: " << result.cost << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<uint32_t>(result.expr.atom));
    EXPECT_EQ(std::get<uint32_t>(result.expr.atom), register_string_in_lookup("A"));
}

TEST(Integration, CyclicTermsThatDoNotExplode) {
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("A * (Inv(A) * A)"));
    std::vector<Rewrite> rules = {
        mul_assoc,
        invert_cancel_right,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 200}});
    int i = 20;
    while (i-- > 0) {
        if (!rewriter.apply_one_iteration()) {
            break;
        }
        // egraph.to_img("cyclic_" + std::to_string(20 - i), "svg");
    }
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<uint32_t>(result.expr.atom));
    EXPECT_EQ(std::get<uint32_t>(result.expr.atom), register_string_in_lookup("A"));
}

TEST(Integration, MatrixChainSymbolicSizes) {
    EGraph egraph(get_property_table_with_symbolic_shapes());

    const Expression root_expr("A * B * C * D * E * F * G");
    const Id root_id = egraph.add_expression(root_expr);
    Rewriter rewriter(egraph, {mul_assoc}, EGraphConfig{.rewrite = {.node_limit = 1000}});
    rewriter.apply_rewrites();

    // egraph.to_img("MatrixChain", "svg");
    Extractor extractor(egraph);

    const auto symbolic_results = extractor.extract_symbolic(root_id);
    std::vector<Expression> candidate_expressions;
    candidate_expressions.reserve(symbolic_results.size());
    std::ranges::transform(symbolic_results, std::back_inserter(candidate_expressions), &ExtractionResult::expr);

    ASSERT_FALSE(candidate_expressions.empty());

    // for (auto expression : candidate_expressions) {
    //     std::cout << "Candidate expression: " << expression.to_string() << std::endl;
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
            //     std::cout << "Extracted Expression: " << extracted_expr.to_string() << std::endl;
            // }
            expression_seen[index] = true;
        }
    }

    const auto matched_count = std::ranges::count(expression_seen, true);
    std::cout << "Matched " << matched_count << " out of " << candidate_expressions.size() << " possible expressions."
              << std::endl;
    SUCCEED();
}

TEST(Integration, SimpleDiagram) {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("B", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("C", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("D", {.shape = std::make_pair(3, 3)});
    EGraph egraph(pt);
    egraph.add_expression(Expression("(A + B) * (C + D)"));
    std::vector<Rewrite> rules = {commute_add, mul_distribute_left, mul_distribute_right};
    Rewriter rewriter = Rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000}});
    rewriter.apply_rewrites();
    egraph.to_dot_file("simple_diagram.dot");
    egraph.to_img("simple_diagram", "svg");
}

TEST(Integration, VerySimpleDiagram) {
    PropertyTable pt;
    pt.add_or_update_property_entry("a", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("b", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("c", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("d", {.shape = std::make_pair(3, 3)});
    EGraph egraph(pt);
    egraph.add_expression(Expression("(a + b)*(c + d)"));
    egraph.add_expression(Expression("(a+b)*(d+c)"));
    egraph.to_dot_file("diagram.dot");
    egraph.to_img("very_simple_diagram", "svg");
}

TEST(Integration, ExpressionMapToManyKernelSequences) {
    EGraph egraph(get_property_table());
    egraph.add_expression(Expression("Sol(Get(QR(X),1), Tr(Get(QR(X),0)) * y)"));
    Rewriter rewriter(
        egraph, build_rewrite_sets({"lowering"}),
        EGraphConfig{.rewrite = {.node_limit = 1000, .enable_backoff = true}});
    rewriter.apply_rewrites();
    Pruner::prune_symbolic_when_kernel_available(egraph);
    egraph.to_img("many_kernels", "svg");
}