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
    EXPECT_TRUE(std::holds_alternative<uint32_t>(result.expr.atom));
    EXPECT_EQ(std::get<uint32_t>(result.expr.atom), register_string_in_lookup("A"));
}

TEST_F(ExtractorTest, RewriteAndExtract) {

    Id root_id = egraph.add_expression(Expression("(A * Zero) + Z"));

    Pattern p1_lhs("?x * Zero");
    Pattern p1_rhs("Zero");
    Rewrite r1{"mul_zero", p1_lhs, p1_rhs};
    Pattern p2_lhs("Zero + ?x");
    Pattern p2_rhs("?x");
    Rewrite r2{"add_zero", p2_lhs, p2_rhs};

    std::vector<Rewrite> rules = {r1, r2};
    Rewriter rewriter(egraph, rules, RewriteConfig{.node_limit = 1000});
    rewriter.apply_rewrites();

    ExtractionResult result = extractor.extract(root_id);
    EXPECT_EQ(result.cost, Cost(0.0));

    EXPECT_TRUE(std::holds_alternative<uint32_t>(result.expr.atom));
    EXPECT_EQ(std::get<uint32_t>(result.expr.atom), register_string_in_lookup("Z"));
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
    EXPECT_EQ(std::get<double>(result.cost), 84.0);

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

    Id id_root = egraph.add_node(make_op(Op::Add, {id_mul_yx, mul_inv_d})); // 2x2
    egraph.to_img("extractor_test_initial", "svg");

    auto result = extractor.extract(id_root);
    EXPECT_TRUE(std::holds_alternative<double>(result.cost));
    EXPECT_EQ(std::get<double>(result.cost), 44.0);
}

TEST_F(ExtractorTest, ExtractSymbolicSingleDag) {

    Id id_root = egraph.add_expression(Expression("Tr(M) * n"));

    auto results = extractor.extract_symbolic(id_root);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].expr.to_string(), "Tr(M) * n");
    ASSERT_TRUE(std::holds_alternative<SymbolicCost>(results[0].cost));

    SymbolicCost expected;
    expected[Monomial{{"A", "B", "1"}}] = 2.0;
    expected[Monomial{{"A", "B"}}] = 1.0;
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
        expected[Monomial{{"A", "B"}}] = 1.0;
        EXPECT_EQ(std::get<SymbolicCost>(r.cost), expected);
    }

    EXPECT_TRUE(exprs.contains("Tr(M) * n"));
    EXPECT_TRUE(exprs.contains("Tr(P) * n"));
}

TEST_F(ExtractorTest, ExtractTopNumericDags) {
    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_x = egraph.add_node(make_symbol("X"));
    Id id_y = egraph.add_node(make_symbol("Y"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_x, id_y}));

    egraph.union_classes(id_a, id_mul);
    egraph.rebuild();

    auto results = extractor.extract(id_a, 2);
    ASSERT_EQ(results.size(), 2);
    EXPECT_TRUE(std::holds_alternative<double>(results[0].cost));
    EXPECT_TRUE(std::holds_alternative<double>(results[1].cost));
    EXPECT_LE(std::get<double>(results[0].cost), std::get<double>(results[1].cost));

    std::set<std::string> exprs;
    for (const auto &result : results) {
        exprs.insert(result.expr.to_string());
    }
    EXPECT_TRUE(exprs.contains("A"));
    EXPECT_TRUE(exprs.contains("X * Y"));
}

TEST_F(ExtractorTest, ExtractTopDagsWithSizeBindings) {
    egraph.register_or_update_property("p", MatrixProperty{.shape = std::make_pair(Size("B"), 1)});

    Id id_n = egraph.add_node(make_symbol("n"));
    Id id_m = egraph.add_node(make_symbol("M"));
    Id id_p = egraph.add_node(make_symbol("p"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_m, id_p}));

    egraph.union_classes(id_n, id_mul);
    egraph.rebuild();

    const SizeBindings size_bindings = {{"A", 64}, {"B", 32}};
    auto results = extractor.extract(id_n, 2, size_bindings);
    ASSERT_EQ(results.size(), 2);

    std::set<std::string> exprs;
    for (const auto &result : results) {
        exprs.insert(result.expr.to_string());
    }
    EXPECT_TRUE(exprs.contains("n"));
    EXPECT_TRUE(exprs.contains("M * p"));
}

TEST_F(ExtractorTest, GreedyExtract) {
    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_x = egraph.add_node(make_symbol("X"));
    Id id_y = egraph.add_node(make_symbol("Y"));
    Id id_mul = egraph.add_node(make_op(Op::Mul, {id_x, id_y}));

    egraph.union_classes(id_a, id_mul);
    egraph.rebuild();

    auto result = extractor.tree_extract(id_a);
    // 'A' has local cost 0, 'X * Y' has local cost > 0.
    EXPECT_EQ(result.cost, Cost(0.0));
    EXPECT_EQ(result.expr.to_string(), "A");
}

TEST_F(ExtractorTest, GreedyExtractIgnoresSharing) {
    egraph.register_or_update_property("X", MatrixProperty{.shape = Shape{10, 10}});
    egraph.register_or_update_property("A", MatrixProperty{.shape = Shape{10, 10}});
    egraph.register_or_update_property("B", MatrixProperty{.shape = Shape{10, 10}});
    egraph.register_or_update_property("Y", MatrixProperty{.shape = Shape{10, 10}});

    Id id_x = egraph.add_node(make_symbol("X"));
    Id id_a = egraph.add_node(make_symbol("A"));
    Id id_b = egraph.add_node(make_symbol("B"));
    Id id_y = egraph.add_node(make_symbol("Y"));

    Id id_qr_x = egraph.add_node(make_op(Op::Mul, {id_x, id_x}));

    // cost of Add = 100
    Id id_add1 = egraph.add_node(make_op(Op::Add, {id_qr_x, id_a}));
    Id id_add2 = egraph.add_node(make_op(Op::Add, {id_qr_x, id_b}));

    // cost = 2000
    Id id_root1 = egraph.add_node(make_op(Op::Mul, {id_add1, id_add2}));
    // DAG cost: Mul(2000) + Add1(100) + Add2(100) + MulXX(2000) = 4200.
    // Tree cost: Mul(2000) + Add1(100) + Add2(100) + 2 * MulXX(2000) = 6200.

    // Cost = 2000 + 2000 + 2000 = 6000.
    Id id_yy = egraph.add_node(make_op(Op::Mul, {id_y, id_y}));
    Id id_ab = egraph.add_node(make_op(Op::Mul, {id_a, id_b}));
    Id id_root2 = egraph.add_node(make_op(Op::Mul, {id_yy, id_ab}));

    egraph.union_classes(id_root1, id_root2);
    egraph.rebuild();

    // Greedy should pick root2
    // 6000 < 6200
    auto greedy_result = extractor.tree_extract(id_root1);
    EXPECT_EQ(greedy_result.expr.to_string(), "(Y * Y) * (A * B)");

    // Regular extract should pick root1
    // 4200 < 6000
    auto dag_result = extractor.extract(id_root1);
    EXPECT_EQ(dag_result.expr.to_string(), "(X * X + A) * (X * X + B)");
}
