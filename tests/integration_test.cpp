#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include "rewrite_rules.h"
#include "utils.h"
#include "test_helper.h"

TEST(Integration, MatrixPartialSet)
{
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Invert(Mul(A, Z)), A), Z), X)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, mul_identity_right};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'X'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "X");
}

TEST(Integration, SimplifyComplexMatrixChain)
{
    EGraph egraph(get_property_table());

    Expression root_expr("Mul(Transpose(Mul(A, X)), Invert(Transpose(A)))");
    Id root_id = egraph.add_expression(root_expr);
    std::cout << "Initial EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;

    std::vector<Rewrite> rules = {
        mul_identity_left,
        mat_transpose_prod,
        invert_cancel_right,
        mul_assoc_right,

    };
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(20);

    Extractor extractor(egraph);

    ExtractionResult result = extractor.extract(root_id);

    std::string result_str = result.expr.to_string();
    std::cout << "Best extracted expression: " << result_str << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Final EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;
    Expression expected("Transpose(X)");
    Id expected_id = egraph.add_expression(expected);

    EXPECT_EQ(egraph.find_class_id(root_id), egraph.find_class_id(expected_id))
        << "The expression should be equivalent to Transpose(X)";
    // Should be Transpose(X)
    EXPECT_EQ(atom_to_string(result.expr.atom), "Transpose");
    ASSERT_EQ(result.expr.children.size(), 1);
    EXPECT_EQ(atom_to_string(result.expr.children[0].atom), "X");
}

TEST(Integration, MinimalRealisticExplosionRules)
{
    EGraph egraph(get_property_table());

    // A (A-1A)
    // (AA-1)A
    auto id = egraph.add_expression(Expression("Mul(Mul(Invert(A), A), A)"));
    std::vector<Rewrite> rules = {
        mul_assoc_right,
        invert_cancel_left,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 200);
    int i = 20;
    while (i-- > 0)
    {
        rewriter.apply_one_iteration();
    }
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    // Should extract 'A'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, QRSolveLLSProblem)
{
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Invert( Mul(Transpose(X),X) ) , Transpose(X) ), y)"));

    std::vector<Rewrite> rules = {
        QR_introduction,
        mat_transpose_prod,
        invert_mat_prod,
        orthonormal_transpose,
        invert_cancel_left,
        mul_identity_right,
        mul_assoc_left,
        mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 10000);
    int iteration = 0;
    while (rewriter.apply_one_iteration())
    {
        iteration++;
        auto should_be_root_id = egraph.add_expression(Expression("Mul(Mul(Invert(Get(QR(X), 1)), Transpose(Get(QR(X), 0))), y)"));
        if (egraph.find_class_id(id) == egraph.find_class_id(should_be_root_id))
        {
            std::cout << "Found the QR-based solution in iteration " << iteration << "! Num nodes: " << egraph.num_nodes() << std::endl;
            return;
        }
    }
    FAIL() << "Did not find the QR-based solution within the nodes limit.";
}