#include "e_graph.h"
#include "extractor.h"
#include "rewrite_rules.h"
#include "rewriter.h"
#include "test_helpers.h"
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
            Extractor extractor(egraph);
            auto result = extractor.extract(id, {{"A", 100}, {"B", 50}});
            std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
            std::cout << "Cost: " << result.cost << std::endl;
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
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Best extracted expression: " << result.expr.to_human_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}