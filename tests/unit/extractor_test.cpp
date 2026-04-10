#include "e_graph.h"
#include "extractor.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <gtest/gtest.h>

TEST_F(ExtractorTest, CheaperExtraction) {
    // Expr: A * Identity
    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_identity = egraph.add_node(make_symbol("I_3x3"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_a, id_identity}));

    egraph.union_classes(id_mul, id_a);
    egraph.rebuild();

    auto result = extractor.extract(id_mul);
    // Should extract 'a'
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "A");
}

TEST_F(ExtractorTest, RewriteAndExtract) {

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

    ExtractionResult result = extractor.extract(root_id);
    EXPECT_EQ(result.cost, Cost(0.0));

    EXPECT_TRUE(std::holds_alternative<std::string>(result.expr.atom));
    EXPECT_EQ(std::get<std::string>(result.expr.atom), "Z");
}

TEST_F(ExtractorTest, SharedSubexpressionDAGCost) {

    Id id_x = egraph.add_node(make_symbol("X"));
    Id id_tr_x = egraph.add_node(make_op(Op::Tr, {id_x}));

    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_z = egraph.add_node(make_symbol("Z"));

    Id id_mul1 = egraph.add_node(make_op(Op::Mul, {id_tr_x, id_a}));

    Id id_mul2 = egraph.add_node(make_op(Op::Mul, {id_tr_x, id_z}));

    Id id_root = egraph.add_node(make_op(Op::Add, {id_mul1, id_mul2}));

    auto result = extractor.extract(id_root);

    EXPECT_TRUE(std::holds_alternative<double>(result.cost));
    EXPECT_EQ(std::get<double>(result.cost), 78.0);

    EXPECT_TRUE(std::holds_alternative<Op>(result.expr.atom));
    EXPECT_EQ(std::get<Op>(result.expr.atom), Op::Add);
    EXPECT_EQ(result.expr.children.size(), 2);
}

TEST_F(ExtractorTest, ExpensiveSharedWinsWithHighReuse) {

    Id id_x = egraph.add_node(make_symbol("X")); // 3x2
    Id id_y = egraph.add_node(make_symbol("Y")); // 2x3
    Id id_d = egraph.add_node(make_symbol("D")); // 2x2

    Id id_mul_yx = egraph.add_node(make_op(Op::Mul, {id_y, id_x}));  // 2x2, cost 12
    Id inv_d = egraph.add_node(make_op(Op::Inv, {id_d}));            // 2x2, cost 64
    Id mul_inv_d = egraph.add_node(make_op(Op::Mul, {inv_d, id_d})); // 2x2, cost 8
    egraph.union_classes(id_mul_yx, inv_d);

    // Root expression: Add(Mul(Y, X), Mul(Inv(D), D))
    Id id_root = egraph.add_node(make_op(Op::Add, {id_mul_yx, mul_inv_d})); // 2x2
    egraph.to_img("extractor_test_initial", "svg");

    auto result = extractor.extract(id_root);
    EXPECT_TRUE(std::holds_alternative<double>(result.cost));
    EXPECT_EQ(std::get<double>(result.cost), 44.0);
}

TEST_F(ExtractorTest, ExtractSymbolicSingleDag) {

    Id id_root = egraph.add_expression(Expression("Mul(Tr(M), n)"));

    auto results = extractor.extract_symbolic(id_root);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].expr.to_string(), "Mul(Tr(M), n)");
    ASSERT_TRUE(std::holds_alternative<SymbolicCost>(results[0].cost));

    SymbolicCost expected;
    expected[Monomial{{"A", "B", "1"}}] = 2.0; // Mul(Tr(M), n), with Tr local cost = 0
    EXPECT_EQ(std::get<SymbolicCost>(results[0].cost), expected);
}

TEST_F(ExtractorTest, ExtractSymbolicMultipleDags) {

    egraph.register_or_update_property("P", MatrixProperty{.shape = std::make_pair(Size("A"), Size("B"))});

    Id id_m = egraph.add_node(make_symbol("M"));
    Id id_p = egraph.add_node(make_symbol("P"));
    Id id_n = egraph.add_node(make_symbol("n"));

    Id id_tr_m = egraph.add_node(make_op(Op::Tr, {id_m}));
    Id id_tr_p = egraph.add_node(make_op(Op::Tr, {id_p}));
    egraph.union_classes(id_tr_m, id_tr_p);
    egraph.rebuild();

    Id id_root = egraph.add_node(make_op(Op::Mul, {id_tr_m, id_n}));

    auto results = extractor.extract_symbolic(id_root);
    ASSERT_EQ(results.size(), 2);

    std::set<std::string> exprs;
    for (const auto &r : results) {
        exprs.insert(r.expr.to_string());
        ASSERT_TRUE(std::holds_alternative<SymbolicCost>(r.cost));
        SymbolicCost expected;
        expected[Monomial{{"A", "B", "1"}}] = 2.0;
        EXPECT_EQ(std::get<SymbolicCost>(r.cost), expected);
    }

    EXPECT_TRUE(exprs.contains("Mul(Tr(M), n)"));
    EXPECT_TRUE(exprs.contains("Mul(Tr(P), n)"));
}
