#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include "rewrite_rules.h"
#include "utils.h"

TEST(Integration, MatrixPartialSet)
{
    EGraph egraph;

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Invert(Mul(A, F)), A), F), D)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, mul_identity_right};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'D'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "D");
}

TEST(Integration, MatrixFullSet)
{
    EGraph egraph;

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Invert(Mul(A, F)), A), F), D)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, invert_cancel_right, mul_identity_right, mul_identity};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(6);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'D'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "D");
}

TEST(Integration, SimplifyComplexMatrixChain)
{
    EGraph egraph;

    Expression root_expr("Mul(Transpose(Mul(E, C)), Invert(Transpose(E)))");
    Id root_id = egraph.add_expression(root_expr);
    std::cout << "Initial EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;

    std::vector<Rewrite> rules = {
        mat_transpose_prod,
        invert_cancel_right,
        mul_assoc_right,
        mul_identity,
    };
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    std::cout << "EGraph size after saturation: " << egraph.num_nodes() << " nodes." << std::endl;

    Extractor extractor(egraph);
    ExtractionResult result = extractor.extract(root_id);

    std::string result_str = result.expr.to_string();
    std::cout << "Best extracted expression: " << result_str << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;

    Expression expected("Transpose(C)");
    Id expected_id = egraph.add_expression(expected);

    EXPECT_EQ(egraph.find_class_id(root_id), egraph.find_class_id(expected_id))
        << "The expression should be equivalent to Transpose(C)";
    // Should be Transpose(B)
    EXPECT_EQ(atom_to_string(result.expr.atom), "Transpose");
    ASSERT_EQ(result.expr.children.size(), 1);
    EXPECT_EQ(atom_to_string(result.expr.children[0].atom), "C");

    egraph.print_egraph();
}