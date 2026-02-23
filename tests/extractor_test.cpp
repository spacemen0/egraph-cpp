#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include "test_helper.h"

TEST(Extractor, CheaperExtraction)
{
    EGraph egraph(get_property_table());
    // Expr: A * Identity
    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_identity = egraph.add_node(make_symbol("I_3x3"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_a, id_identity}));

    egraph.union_classes(id_mul, id_a);
    egraph.rebuild();

    Extractor extractor(egraph);
    auto result = extractor.extract(id_mul);
    // Should extract 'a'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST(Extractor, RewriteAndExtract)
{
    EGraph egraph(get_property_table());

    Id root_id = egraph.add_expression(Expression("Add(Mul(A, Zero), Z)"));

    // Mul(x, Zero) -> Zero
    Pattern p1_lhs("Mul(?x, Zero)");
    Pattern p1_rhs("Zero");
    Rewrite r1{"mul_zero", p1_lhs, p1_rhs};

    // Add(Zero, x) -> x
    Pattern p2_lhs("Add(Zero, ?x)");
    Pattern p2_rhs("?x");
    Rewrite r2{"add_zero", p2_lhs, p2_rhs};

    std::vector<Rewrite> rules = {r1, r2};
    Rewriter rewriter(egraph, rules, 1000);
    rewriter.apply_rewrites();

    Extractor extractor(egraph);
    ExtractionResult result = extractor.extract(root_id);
    egraph.to_dot_file("extractor_test.dot");
    EXPECT_EQ(result.cost, 1.0);

    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "Z");
}
