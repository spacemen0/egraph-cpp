#include "e_graph.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST(Integration, GLSNumeric) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair(3, 3), .flags = {.is_symmetric = true, .is_positive_definite = true}});
    auto id = egraph.add_expression(Expression("(Inv(Tr(X) * Inv(M) * X) * (Tr(X) * Inv(M))) * y"));

    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000, .enable_backoff = true}});
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, EGraphConfig{.enable_logging = true});
    Pruner::prune_symbolic_when_kernel_available(egraph);
    auto result = extractor.extract(id, 10);
    for (const auto &r : result) {
        std::cout << "Candidate expression: " << r.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << r.cost << std::endl;
    }
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}

TEST(Integration, GLSSymbolic) {
    PropertyTable pt;
    pt.add_or_update_property_entry("X", {.shape = std::make_pair("A", "B"), .flags = {.is_positive_definite = true}});
    pt.add_or_update_property_entry(
        "M", {.shape = std::make_pair("A", "A"), .flags = {.is_symmetric = true, .is_positive_definite = true}});
    pt.add_or_update_property_entry("y", {.shape = std::make_pair("A", 1)});
    EGraph egraph(pt);
    auto id = egraph.add_expression(Expression("(Inv(Tr(X) * Inv(M) * X) * (Tr(X) * Inv(M))) * y"));

    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, EGraphConfig{.rewrite = {.node_limit = 1000, .enable_backoff = true}});
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, EGraphConfig{.enable_logging = true});
    auto result = extractor.extract(id, 10, {{"A", 100}, {"B", 20}});
    for (const auto &r : result) {
        std::cout << "Candidate expression: " << r.expr.to_string(true) << std::endl;
        std::cout << "Cost: " << r.cost << std::endl;
    }
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}