#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewrite.h"
#include "rewrite_rules.h"

TEST(Integration, MatrixPartialSet)
{
    EGraph egraph;

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Invert(Mul(A, F)), A), F), D)"));
    apply_rewrites(egraph, {mul_assoc_right, invert_cancel_left, mul_identity_right});
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'D'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "D");
}
