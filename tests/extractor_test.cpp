#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewrite.h"
#include "test_helper.h"

TEST(Extractor, CheaperExtraction)
{
    EGraph egraph;
    // Expr: a * 1
    Id id_a = egraph.add_node(make_symbol("a"));
    Id id_1 = egraph.add_node(make_symbol("1"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_a, id_1}));

    egraph.union_classes(id_mul, id_a);
    egraph.rebuild();

    Extractor extractor(egraph);
    auto result = extractor.extract(id_mul);
    // Should extract 'a'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "a");
}

TEST(Extractor, RewriteAndExtract)
{
    EGraph egraph;

    Id root_id = egraph.add_expression(Expression("Add(Mul(a, Zero), b)"));

    // Mul(x, Zero) -> Zero
    Pattern p1_lhs("Mul(x, Zero)");
    Pattern p1_rhs("Zero");
    Rewrite r1{"mul_zero", p1_lhs, p1_rhs};

    // Add(Zero, x) -> x
    Pattern p2_lhs("Add(Zero, x)");
    Pattern p2_rhs("x");
    Rewrite r2{"add_zero", p2_lhs, p2_rhs};

    apply_rewrites(egraph, {r1, r2});

    Extractor extractor(egraph);
    ExtractionResult result = extractor.extract(root_id);

    EXPECT_EQ(result.cost, 1.0);

    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "b");
}
