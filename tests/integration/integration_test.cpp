#include <gtest/gtest.h>
#include <algorithm>
#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include "rewrite_rules.h"
#include "utils.h"
#include "test_helpers.h"

TEST(Integration, MatrixPartialSet)
{
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Inv(Mul(A, Z)), A), Z), X)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, mul_identity_right};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "X");
}

TEST(Integration, SimplifyComplexMatrixChain)
{
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

    Extractor extractor(egraph);
    auto results = extractor.extract_symbolic(root_id);
    for (const auto &result : results)
    {
        std::cout << "Extracted expression: " << result.expr.to_human_string() << std::endl;
        std::cout << "Cost: " << result.cost << std::endl;
    }
}

TEST(Integration, MinimalRealisticExplosionRules)
{
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul(Mul(Inv(A), A), A)"));
    auto mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))", nullptr, nullptr, 10);
    std::vector<Rewrite> rules = {
        mul_assoc_right,
        invert_cancel_left,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 2000, false, true);
    rewriter.apply_rewrites(20);

    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, CyclicTermsThatDoNotExplode)
{
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(A, Mul(Inv(A), A))"));
    std::vector<Rewrite> rules = {
        mul_assoc_left,
        invert_cancel_right,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 200);
    int i = 20;
    while (i-- > 0)
    {
        if (!rewriter.apply_one_iteration())
        {
            break;
        }
        // egraph.to_img("cyclic_" + std::to_string(20 - i), "svg");
    }
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, MatrixChainSymbolicSizes)
{
    EGraph egraph(get_property_table_with_symbolic_shapes());

    Expression root_expr("Mul(Mul(Mul(Mul(A, B), C), D), E)");
    Id root_id = egraph.add_expression(root_expr);
    Rewriter rewriter(egraph, {mul_assoc_left, mul_assoc_right}, 1000);
    rewriter.apply_rewrites();
    Extractor extractor(egraph);
    std::vector<Expression> possible_expressions;
    auto results = extractor.extract_symbolic(root_id);
    std::ranges::transform(results, std::back_inserter(possible_expressions), &ExtractionResult::expr);
    SUCCEED();
}