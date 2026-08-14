#include "evaluator.h"

#include "e_graph.h"
#include "expression.h"
#include "extractor.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include "test_helpers.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

TEST(Integration, OLSNumeric) {
    EGraph egraph(get_property_table());
    auto root_expr =
        inverse(transpose(Expression("J")) * Expression("J")) * transpose(Expression("J")) * Expression("k");
    auto id = egraph.add_expression(root_expr);
    auto config = initialize_config_for_expression(root_expr);
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    Rewriter rewriter(egraph, rules, config);
    constexpr int max_iterations = 10;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        if (!rewriter.apply_one_iteration()) {
            break;
        }
    }
    auto alternative_expression = Expression("Sol(Get(QR(J),1), Tr(Get(QR(J),0)) * k)");
    auto alternative_id = egraph.add_expression(alternative_expression);
    ASSERT_TRUE(egraph.find_class_id(alternative_id) == egraph.find_class_id(id));
    // egraph.to_img("OLS_numeric", "svg");
    Pruner::prune_symbolic_when_kernel_available(egraph);
    std::cout << "Doing extraction, num of nodes: " << egraph.num_nodes() << std::endl;
    Extractor extractor(egraph, config);

    auto result = extractor.extract(id);
    std::cout << "Candidate expression: " << result.expr.to_string(false) << std::endl;
    std::cout << "Cost: " << result.cost << std::endl;
    EXPECT_EQ(
        result.expr.to_string(false), "Trsm_LN(Get(Potrf_U(Syrk_T(J, Zero_20x20)), 0), Trsm_LT(Get(Potrf_U(Syrk_T(J, "
                                      "Zero_20x20)), 0), Gemv_T(J, k, Zero_20x1)))");

    DataBindings data_bindings = {{"J", generate_random_vector(30 * 20)}, {"k", generate_random_vector(30)}};
    Evaluator evaluator(egraph, result, nullptr, data_bindings);
    const auto &out = evaluator.evaluate();
    std::cout << "EVALUATOR OLS NUMERIC OUTPUT (first 5): ";
    for (int i = 0; i < 5; ++i)
        std::cout << out[i] << " ";
    std::cout << std::endl;
}

TEST(Integration, OLSSymbolic) {
    auto start_total = std::chrono::high_resolution_clock::now();

    EGraph egraph(get_property_table());

    Expression root_expr("Inv(Tr(M) * M) * Tr(M) * n");

    auto start_add = std::chrono::high_resolution_clock::now();
    auto id = egraph.add_expression(root_expr);
    auto end_add = std::chrono::high_resolution_clock::now();

    auto start_rules = std::chrono::high_resolution_clock::now();
    std::vector<Rewrite> rules = build_rewrite_sets({"complete"});
    auto end_rules = std::chrono::high_resolution_clock::now();
    auto config = initialize_config_for_expression(root_expr);
    Rewriter rewriter(egraph, rules, config);
    auto start_rewrite = std::chrono::high_resolution_clock::now();
    rewriter.apply_rewrites(10);
    auto end_rewrite = std::chrono::high_resolution_clock::now();

    Extractor extractor(egraph, config);
    Rewriter kernel_rewriter(
        egraph, build_rewrite_sets({"lowering"}),
        config); // Use the same config for kernel rewriting to ensure consistent behavior});
    kernel_rewriter.apply_rewrites();
    Pruner::prune_symbolic_when_kernel_available(egraph);
    auto start_extract = std::chrono::high_resolution_clock::now();
    auto result = extractor.extract(id, {{"A", 30}, {"B", 20}});
    std::cout << "EVALUATOR OLS SYMBOLIC OUTPUT: " << result.expr.to_string(false) << std::endl;
    auto end_extract = std::chrono::high_resolution_clock::now();

    auto end_total = std::chrono::high_resolution_clock::now();

    std::cout << "\n--- OLSSymbolic Runtime Statistics ---" << std::endl;
    std::cout << "Add Expression: " << std::chrono::duration<double, std::milli>(end_add - start_add).count() << " ms"
              << std::endl;
    std::cout << "Build Rules:    " << std::chrono::duration<double, std::milli>(end_rules - start_rules).count()
              << " ms" << std::endl;
    std::cout << "Apply Rewrites: " << std::chrono::duration<double, std::milli>(end_rewrite - start_rewrite).count()
              << " ms" << std::endl;
    std::cout << "Extraction:     " << std::chrono::duration<double, std::milli>(end_extract - start_extract).count()
              << " ms" << std::endl;
    std::cout << "Total Time:     " << std::chrono::duration<double, std::milli>(end_total - start_total).count()
              << " ms" << std::endl;
    std::cout << "--------------------------------------\n" << std::endl;

    std::cout << "ACTUAL RESULT: " << result.expr.to_string(false) << std::endl;
    std::string actual = result.expr.to_string(false);
    std::string expected_l = "Trsm_LT(Get(Potrf_L(Syrk_T(M, Zero_BxB)), 0), Trsm_LN(Get(Potrf_L(Syrk_T(M, "
                             "Zero_BxB)), 0), Gemv_T(M, n, Zero_Bx1)))";
    std::string expected_u = "Trsm_LN(Get(Potrf_U(Syrk_T(M, Zero_BxB)), 0), Trsm_LT(Get(Potrf_U(Syrk_T(M, "
                             "Zero_BxB)), 0), Gemv_T(M, n, Zero_Bx1)))";
    EXPECT_TRUE(actual == expected_l || actual == expected_u) << "Actual expression: " << actual;
}
