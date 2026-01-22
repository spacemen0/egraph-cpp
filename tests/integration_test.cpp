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
        mul_identity,
        mat_transpose_prod,
        invert_cancel_right,
        mul_assoc_right,

    };
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites(20);
    // egraph.print_egraph();
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

TEST(Integration, MinimalExplosionRules)
{
    EGraph egraph(get_property_table());

    Expression root("Invert(A)");
    Id root_id = egraph.add_expression(root);
    Rewrite explosion_rule = make_rewrite("inv_pad_identity", "Invert(?a)", "Invert(Mul(?a, Identity))", nullptr, [](EGraph &g, const Substitution &s)
                                          { Id identity = make_identity_for(g, s, "a"); 
                                            Id mul_node = g.add_node(ENode({s.at("a"), identity}, Op::Mul));
                                            return g.add_node(ENode({mul_node}, Op::Invert)); });
    Rewriter rewriter(egraph, {explosion_rule}, 100);

    rewriter.apply_rewrites();
    // egraph.print_egraph();
    std::cout << "Final EGraph size: " << egraph.num_nodes() << " nodes." << std::endl;

    Rewrite clean_up_rule = make_rewrite("identity_cancel", "Mul(?a, ?b)", "?a", [](const EGraph &g, const Substitution &s)
                                         { return is_identity(s, g, "b"); });
    Rewriter cleaner(egraph, {clean_up_rule}, 200);
    cleaner.apply_rewrites();
    // egraph.print_egraph();
    std::cout << "Final EGraph size after clean-up: " << egraph.num_nodes() << " nodes." << std::endl;
}

TEST(Integration, MinimalRealisticExplosionRules)
{
    EGraph egraph(get_property_table());

    auto id = egraph.add_expression(Expression("Mul(Mul(A, Invert(A)), A)"));
    std::vector<Rewrite> rules = {
        mul_assoc_right,
        invert_cancel_left,
        invert_cancel_right,
        mul_identity_right,
    };
    Rewriter rewriter(egraph, rules, 20);
    rewriter.apply_rewrites(30);
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    std::cout << "Num nodes after rewriting: " << egraph.num_nodes() << std::endl;
    egraph.print_egraph();
    // Should extract 'A'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}