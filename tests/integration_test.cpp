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

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Inv(Mul(A, Z)), A), Z), X)"));
    std::vector<Rewrite> rules = {mul_assoc_right, invert_cancel_left, mul_identity_right};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(4);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'X'
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "X");
}

TEST(Integration, SimplifyComplexMatrixChain)
{
    EGraph egraph(get_property_table());

    Expression root_expr("Mul(Tr(Mul(A, X)), Inv(Tr(A)))");
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
    Expression expected("Tr(X)");
    Id expected_id = egraph.add_expression(expected);

    EXPECT_EQ(egraph.find_class_id(root_id), egraph.find_class_id(expected_id))
        << "The expression should be equivalent to Tr(X)";
    // Should be Tr(X)
    EXPECT_EQ(atom_to_string(result.expr.atom), "Tr");
    ASSERT_EQ(result.expr.children.size(), 1);
    EXPECT_EQ(atom_to_string(result.expr.children[0].atom), "X");
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
    int iteration_number = 20;
    rewriter.apply_rewrites(iteration_number);
    // int i = iteration_number;
    // while (i-- > 0)
    // {
    //     rewriter.apply_one_iteration();
    //     egraph.to_img("explosion_" + std::to_string(iteration_number - i), "svg");
    // }
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    // Should extract 'A'
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, CyclicTermsThatDoNotExplode)
{
    EGraph egraph(get_property_table());

    // A (A-1A)
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
        egraph.to_img("cyclic_" + std::to_string(20 - i), "svg");
    }
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;

    // Should extract 'A'
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Integration, OLSSymbolic)
{
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(M), M) ) , Tr(M) ), n)"));

    std::vector<Rewrite> rules = {
        qr_invert,
        qr_inner_invert,
        mat_transpose_prod,
        invert_mat_prod,
        orthonormal_transpose,
        invert_cancel_left,
        mul_identity_right,
        mul_assoc_left,
        mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
    int iteration = 0;
    egraph.to_img("qr_0", "svg");
    while (rewriter.apply_one_iteration())
    {
        iteration++;
        auto should_be_root_id = egraph.add_expression(Expression("Mul(Mul(Inv(Get(QR(M), 1)), Tr(Get(QR(M), 0))), n)"));
        egraph.to_img("qr_" + std::to_string(iteration), "svg");
        if (egraph.find_class_id(id) == egraph.find_class_id(should_be_root_id))
        {
            std::cout << "Found the QR-based solution in iteration " << iteration << "! Num nodes: " << egraph.num_nodes() << std::endl;
            std::cout << "Sample matrix sizes to try out extraction..." << std::endl;
            Extractor extractor(egraph);
            auto result = extractor.extract(id, {{"A", 100}, {"B", 10}, {"c", 1}});
            std::cout << "Best extracted expression: " << result.expr.to_string() << std::endl;
            std::cout << "Cost: " << result.cost << std::endl;
            return;
        }
    }
    FAIL() << "Did not find the QR-based solution within the nodes limit.";
}

TEST(Integration, OLSConcrete)
{
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul ( Mul( Inv( Mul(Tr(X), X) ) , Tr(X) ), y)"));

    std::vector<Rewrite> rules = {
        qr_inner_invert,
        qr_invert,
        mat_transpose_prod,
        invert_mat_prod,
        orthonormal_transpose,
        invert_cancel_left,
        mul_identity_right,
        mul_assoc_left,
        mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
    int iteration = 0;
    rewriter.apply_rewrites(10);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Best extracted expression: " << result.expr.to_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}

TEST(Integration, GLSConcrete)
{
    EGraph egraph(get_property_table());
    auto id = egraph.add_expression(Expression("Mul( Mul( Inv( Mul( Mul(Tr(X), Inv(V)), X) ) , Mul(Tr(X), Inv(V)) ), y)"));

    std::vector<Rewrite> rules = {
        lu_invert,
        llt_invert,
        qr_inner_invert,
        qr_invert,
        mat_transpose_prod,
        invert_mat_prod,
        orthogonal_transpose,
        invert_cancel_left,
        invert_cancel_right,
        mul_identity_left,
        mul_identity_right,
        mul_assoc_left,
        mul_assoc_right,
    };
    Rewriter rewriter(egraph, rules, 1000);
    int iteration = 0;
    rewriter.apply_rewrites(10);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Best extracted expression: " << result.expr.to_string() << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
}