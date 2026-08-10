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
        const std::vector<std::string> &rulesets = {"complete"}, int max_nodes = 5000, bool enable_backoff = true,
        int max_iterations = 30) {
        if (logging) {
            std::cout << "[API] Running rewrite with rulesets: ";
            for (const auto &rs : rulesets)
                std::cout << rs << " ";
            std::cout << "\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, max_nodes, enable_backoff);
        if (max_iterations > 0) {
            rewriter.apply_rewrites(max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void prune_symbolic_when_kernel_available() {
        if (logging) {
            std::cout << "[API] Pruning symbolic nodes\n";
        }
        auto res = Pruner::prune_symbolic_when_kernel_available(egraph);
        if (logging) {
            std::cout << "[Pruner] Symbolic prune removed " << res.nodes_pruned << " nodes.\n";
        }
    }

    void rewrite_and_prune(
        const std::vector<Id> &target_ids, const std::vector<std::string> &rulesets = {"everything_but_lowering"},
        int num_iterations = 5, int rewrite_steps_per_iteration = 30, int prune_samples_per_iteration = 5,
        int max_results_per_binding = 5, int max_nodes = 5000) {
        if (logging) {
            std::cout << "[API] Starting rewrite_and_prune for " << num_iterations << " iterations...\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, max_nodes, true);
        Extractor extractor(egraph, false, 30);
        Pruner pruner(egraph, extractor);

        PruneOptions options{
            .num_iterations = num_iterations,
            .rewrite_steps_per_iteration = rewrite_steps_per_iteration,
            .prune_samples_per_iteration = prune_samples_per_iteration,
            .max_results_per_binding = max_results_per_binding,
            .size_keys = size_keys,
        };
        pruner.rewrite_and_prune(
            target_ids, rewriter, options, nullptr, [this](int iteration, const PruneResult &result) {
            if (logging) {
                std::cout << "[Pruner] Iteration " << iteration + 1 << " finished. Pruned " << result.nodes_pruned
                          << " nodes.\n";
            }
        });
    }
    void lower_to_kernels() {
        if (logging) {
            std::cout << "[API] Lowering to kernels...\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets({"lowering"});
        Rewriter rewriter(egraph, rewrites, 5000, true);
        rewriter.apply_rewrites();
        prune_symbolic_when_kernel_available();
    }
    ExtractionResult extract(Id target_id, const SizeBindings &bindings = {}) {
        if (logging) {
            std::cout << "[API] Extracting concrete expression for target " << target_id << "...\n";
        }
        Extractor extractor(egraph, logging);
        if (bindings.empty()) {
            return extractor.extract(target_id);
        } else {
            return extractor.extract(target_id, bindings);
        }
    }

    ExtractionResult extract_greedy(Id target_id, const SizeBindings &bindings = {}) {
        if (logging) {
            std::cout << "[API] Fast greedy extraction for target " << target_id << "...\n";
        }
        Extractor extractor(egraph, logging);
        return extractor.tree_extract(target_id, bindings);
    }

    ExtractionResult extract_ilp(Id target_id, const SizeBindings &bindings = {}) {
        if (logging) {
            std::cout << "[API] ILP extraction for target " << target_id << "...\n";
        }
        Extractor extractor(egraph, logging);
        return extractor.ilp_extract(target_id, bindings);
    }

    ExtractionResult extract_astar(Id target_id, const SizeBindings &bindings = {}) {
        if (logging) {
            std::cout << "[API] A* extraction for target " << target_id << "...\n";
        }
        Extractor extractor(egraph, logging);
        return extractor.a_star_extract(target_id, bindings);
    }

    std::vector<ExtractionResult> extract_symbolic() { return extract_symbolic(target_id); }

    std::vector<ExtractionResult> extract_symbolic(Id target_id) {
        if (logging) {
            std::cout << "[API] Extracting symbolic expressions for target " << target_id << "...\n";
        }
        Extractor extractor(egraph, true, 30);
        return extractor.extract_symbolic(target_id);
    }

    Expression optimize_concrete(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"complete"}) {
        if (logging) {
            std::cout << "[API] Optimizing concrete expression: " << target_expr.to_string() << "\n";
        }
        Id target_id = add(target_expr);
        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite(rulesets);
        lower_to_kernels();
        return extract(target_id).expr;
    }

    std::vector<double> evaluate_concrete(const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        return evaluate_concrete(target_id, size_bindings, bindings);
    }

    std::vector<double>
    evaluate_concrete(Id target_id, const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        auto result = extract(target_id, size_bindings);
        if (logging) {
            std::cout << "[API] Extracted expression: " << result.expr.to_string() << "\n";
            std::cout << "[API] Evaluating concrete expression...\n";
        }
        Evaluator evaluator(egraph, result, &size_bindings, bindings);
        return evaluator.evaluate();
    }

    void optimize_symbolic(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"everything_but_lowering"}, int max_nodes = 5000) {
        if (logging) {
            std::cout << "[API] Optimizing symbolic expression: " << target_expr.to_string() << "\n";
        }
        target_id = add(target_expr);

        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite_and_prune({target_id}, rulesets);
        lower_to_kernels();
    }

    void print_properties() const { egraph.get_property_table().print_all_properties(); }

    std::optional<Id> find_expr(const Expression &expr) const { return egraph.find_expression_id(expr); }

    void clear() { egraph = EGraph(); }
    void enable_logging() { logging = true; }

  private:
    EGraph egraph;
    Id target_id = 0;
    bool logging = false;
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
