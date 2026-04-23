#include "cost_storage.h"
#include "e_graph.h"
#include "extractor.h"
#include "property_table.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST(Integration, GLSNumeric) {
    EGraph egraph(get_property_table());
    egraph.register_or_update_property(
        "M",
        MatrixProperty{.shape = std::make_pair(3, 3), .flags = {.is_symmetric = true, .is_positive_definite = true}});
    auto id =
        egraph.add_expression(Expression("(Inv(Tr(X) * Inv(M) * X) * (Tr(X) * Inv(M))) * y"));

    std::vector<Rewrite> rules =
        build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality", "solver"});
    CostStorage cost_storage(egraph);
    Rewriter rewriter(egraph, rules, 1000, true);
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, cost_storage, true);
    auto result = extractor.extract(id, 10);
    for (const auto &r : result) {
        std::cout << "Candidate expression: " << r.expr.to_human_string() << std::endl;
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
    auto id =
        egraph.add_expression(Expression("(Inv(Tr(X) * Inv(M) * X) * (Tr(X) * Inv(M))) * y"));

    std::vector<Rewrite> rules =
        build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality", "solver"});
    CostStorage cost_storage(egraph);
    Rewriter rewriter(egraph, rules, 3000, true);
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, cost_storage, true);
    auto result = extractor.extract(id, {{"A", 100}, {"B", 20}}, 10);
    for (const auto &r : result) {
        std::cout << "Candidate expression: " << r.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << r.cost << std::endl;
    }
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}