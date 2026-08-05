#pragma once

#include "e_graph.h"
#include "evaluator.h"
#include "expression.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace EGraphRunner {

// --- Optimization Context ---
class Context {
  public:
    Context(EGraph egraph = EGraph()) : egraph(std::move(egraph)) {}
    Expression define_matrix(const std::string &name, int rows, int cols, const std::vector<std::string> &flags = {}) {
        MatrixProperty prop;
        prop.shape = Shape{rows, cols};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Expression define_matrix(const std::string &name, const std::string &property_str) {
        auto prop = MatrixProperty::from_string(property_str);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Expression define_matrix_symbolic(
        const std::string &name, const std::string &rows_var, const std::string &cols_var,
        const std::vector<std::string> &flags = {}) {
        size_keys.push_back(rows_var);
        size_keys.push_back(cols_var);
        MatrixProperty prop;
        prop.shape = Shape{rows_var, cols_var};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Expression define_matrix_symbolic(
        const std::string &name, const std::string &rows_var, int cols, const std::vector<std::string> &flags = {}) {
        size_keys.push_back(rows_var);
        MatrixProperty prop;
        prop.shape = Shape{rows_var, cols};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Expression define_matrix_symbolic(
        const std::string &name, int rows, const std::string &cols_var, const std::vector<std::string> &flags = {}) {
        size_keys.push_back(cols_var);
        MatrixProperty prop;
        prop.shape = Shape{rows, cols_var};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Id add(const Expression &expr) { return egraph.add_expression(expr); }

    void rewrite(
        const std::vector<std::string> &rulesets = {"complete"}, int max_nodes = 10000, bool enable_backoff = true,
        int max_iterations = 30) {
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, max_nodes, enable_backoff);
        if (max_iterations > 0) {
            rewriter.apply_rewrites(max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void prune_symbolic_when_kernel_available() { Pruner::prune_symbolic_when_kernel_available(egraph); }

    void rewrite_and_prune(
        const std::vector<Id> &target_ids, const std::vector<std::string> &size_keys,
        const std::vector<std::string> &rulesets = {"complete"}, int num_iterations = 8,
        int rewrite_steps_per_iteration = 20, int prune_samples_per_iteration = 20, int max_results_per_binding = 5,
        int max_nodes = 1000) {
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, max_nodes, true);
        CostStorage cost_storage(egraph);
        Extractor extractor(egraph, cost_storage, false, 20);
        Pruner pruner(egraph, extractor);

        PruneOptions options{
            .num_iterations = num_iterations,
            .rewrite_steps_per_iteration = rewrite_steps_per_iteration,
            .prune_samples_per_iteration = prune_samples_per_iteration,
            .max_results_per_binding = max_results_per_binding,
            .size_keys = size_keys,
        };
        pruner.rewrite_and_prune(target_ids, rewriter, options);
    }

    ExtractionResult extract(Id target_id, const SizeBindings &bindings = {}) {
        CostStorage cost_storage(egraph);
        Extractor extractor(egraph, cost_storage);
        if (bindings.empty()) {
            return extractor.extract(target_id);
        } else {
            return extractor.extract(target_id, bindings);
        }
    }

    std::vector<ExtractionResult> extract_symbolic() { return extract_symbolic(target_id); }

    std::vector<ExtractionResult> extract_symbolic(Id target_id) {
        CostStorage cost_storage(egraph);
        Extractor extractor(egraph, cost_storage, true, 20);
        return extractor.extract_symbolic(target_id);
    }

    Expression optimize_concrete(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"complete"}) {
        Id target_id = add(target_expr);
        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite(rulesets);
        rewrite({"lowering"});
        prune_symbolic_when_kernel_available();
        return extract(target_id).expr;
    }

    std::vector<double> evaluate_concrete(const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        auto result = extract(target_id, size_bindings);
        std::cout << "Extracted expression: " << result.expr.to_string() << "\n";
        Evaluator evaluator(egraph, result, &size_bindings, bindings);
        return evaluator.evaluate();
    }
    std::vector<double>
    evaluate_concrete(Id target_id, const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        auto result = extract(target_id, size_bindings);
        std::cout << "Extracted expression: " << result.expr.to_string() << "\n";
        Evaluator evaluator(egraph, result, &size_bindings, bindings);
        return evaluator.evaluate();
    }

    void optimize_symbolic(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"complete"}) {
        target_id = add(target_expr);

        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite_and_prune({target_id}, size_keys, rulesets);
        rewrite({"lowering"}, 50000, false, -1);
        prune_symbolic_when_kernel_available();
    }

    void print_properties() const { egraph.get_property_table().print_all_properties(); }

    std::optional<Id> find_expr(const Expression &expr) const { return egraph.find_expression_id(expr); }

    void clear() { egraph = EGraph(); }

  private:
    EGraph egraph;
    Id target_id = 0;
    std::vector<std::string> size_keys;

    void apply_flags(MatrixProperty &prop, const std::vector<std::string> &flags) {
        for (const auto &f : flags) {
            bool found = false;
            for (const auto &fd : MatrixProperty::flag_descriptors) {
                if (f == fd.label) {
                    prop.flags.*(fd.member) = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::invalid_argument("Unknown matrix property flag: " + f);
            }
        }
    }
};

} // namespace EGraphRunner
