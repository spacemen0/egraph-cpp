#include "cost_storage.h"
#include "e_graph.h"
#include "extractor.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST(Integration, GLSNumeric) {
    EGraph egraph(get_property_table());
    auto id =
        egraph.add_expression(Expression("Mul( Mul( Inv( Mul( Mul(Tr(X), Inv(V)), X) ) , Mul(Tr(X), Inv(V)) ), y)"));

    std::vector<Rewrite> rules = build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality"});
    CostStorage cost_storage(egraph);
    Rewriter rewriter(egraph, rules, 6000);
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id, 10);
    for (const auto &r : result) {
        std::cout << "Candidate expression: " << r.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << r.cost << std::endl;
    }
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}

TEST(Integration, GLSSymbolic) {
    EGraph egraph(get_property_table());
    auto id =
        egraph.add_expression(Expression("Mul( Mul( Inv( Mul( Mul(Tr(M), Inv(v)), M) ) , Mul(Tr(M), Inv(v)) ), n)"));

    std::vector<Rewrite> rules =
        build_rewrite_sets({"factorization", "algebraic", "inverse", "orthogonality", "solver"});
    CostStorage cost_storage(egraph);
    Rewriter rewriter(egraph, rules, 6000, true);
    while (rewriter.apply_rewrites(10))
        ;
    Extractor extractor(egraph, cost_storage);
    auto result = extractor.extract(id, {{"A", 100}, {"B", 20}});
    std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}