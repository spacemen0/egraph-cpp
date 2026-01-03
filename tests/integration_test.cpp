#include <gtest/gtest.h>
#include "e_graph.h"
#include "extractor.h"
#include "rewrite.h"
#include "test_helper.h"

TEST(Integration, MatrixPartialSet)
{
    EGraph egraph;

    // mul-assoc-right: (* (* ?a ?b) ?c) => (* ?a (* ?b ?c))
    Rewrite r1{
        .name = "mul-assoc-right",
        .lhs = {
            .atom = Op::Mul,
            .children = {
                {
                    .atom = Op::Mul,
                    .children = {
                        {.atom = PatternVar{"?a"}},
                        {.atom = PatternVar{"?b"}},
                    },
                },
                {.atom = PatternVar{"?c"}},
            },
        },
        .rhs = {
            .atom = Op::Mul,
            .children = {
                {.atom = PatternVar{"?a"}},
                {
                    .atom = Op::Mul,
                    .children = {
                        {.atom = PatternVar{"?b"}},
                        {.atom = PatternVar{"?c"}},
                    },
                },
            },
        },
    };

    // invert-cancel-left: (* (invert ?a) ?a) => Identity if is_square("?a")
    Rewrite r2{
        .name = "invert-cancel-left",
        .lhs = {
            .atom = Op::Mul,
            .children = {
                {
                    .atom = Op::Invert,
                    .children = {
                        {.atom = PatternVar{"?a"}},
                    },
                },
                {.atom = PatternVar{"?a"}},
            },
        },
        .rhs = {
            .atom = Op::Identity,
        },
    };

    // mul-identity-right: (* Identity ?a) => ?a
    Rewrite r3{
        .name = "mul-identity-right",
        .lhs = {
            .atom = Op::Mul,
            .children = {
                {.atom = Op::Identity},
                {.atom = PatternVar{"?a"}},
            },
        },
        .rhs = {
            .atom = PatternVar{"?a"},
        },
    };

    auto id = egraph.add_expression(Expression("Mul(Mul(Mul(Invert(Mul(A, F)), A), F), D)"));
    apply_rewrites(egraph, {r1, r2, r3});
    Extractor extractor(egraph);
    auto result = extractor.extract(id);
    // Should extract 'D'
    EXPECT_EQ(result.cost, 1.0);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "D");
}