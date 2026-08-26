#pragma once

#include "e_graph.h"
#include "evaluator.h"
#include "expression.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include <chrono>
#include <dlfcn.h>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace egraph {
extern "C" void cblas_dgemm(
    const int Order, const int TransA, const int TransB, const int M, const int N, const int K, const double alpha,
    const double *A, const int lda, const double *B, const int ldb, const double beta, double *C, const int ldc);

namespace EGraphRunner {

// --- Optimization Context ---
class Context {
  public:
    Context(EGraph egraph = EGraph(), EGraphConfig config = EGraphConfig())
        : egraph(std::move(egraph)), config(config) {}

    void set_config(const EGraphConfig &cfg) { config = cfg; }
    EGraphConfig &get_config() { return config; }
    const EGraphConfig &get_config() const { return config; }
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

    void rewrite(const std::vector<std::string> &rulesets = {"complete"}) {
        if (config.enable_logging) {
            std::cout << "[API] Running rewrite with rulesets: ";
            for (const auto &rs : rulesets)
                std::cout << rs << " ";
            std::cout << "\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);

        Rewriter rewriter(egraph, rewrites, config);
        if (config.rewrite.max_iterations > 0) {
            rewriter.apply_rewrites(config.rewrite.max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void prune_symbolic_when_kernel_available() {
        if (config.enable_logging) {
            std::cout << "[API] Pruning symbolic nodes\n";
        }
        auto res = Pruner::prune_symbolic_when_kernel_available(egraph);
        if (config.enable_logging) {
            std::cout << "[Pruner] Symbolic prune removed " << res.nodes_pruned << " nodes.\n";
        }
    }

    void rewrite_and_prune(
        const std::vector<Id> &target_ids, const std::vector<std::string> &rulesets = {"everything_but_lowering"}) {
        if (config.enable_logging) {
            std::cout << "[API] Starting rewrite_and_prune...\n";
        }

        PrunerConfig pruner_cfg = config.pruner;

        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, config);
        Extractor extractor(egraph, config);
        Pruner pruner(egraph, extractor);

        pruner.rewrite_and_prune(
            target_ids, rewriter, pruner_cfg, size_keys, nullptr, [this](int iteration, const PruneResult &result) {
            if (config.enable_logging) {
                std::cout << "[Pruner] Iteration " << iteration + 1 << " finished. Pruned " << result.nodes_pruned
                          << " nodes.\n";
            }
        });
    }

    void lower_to_kernels() {
        if (config.enable_logging) {
            std::cout << "[API] Lowering to kernels...\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets({"lowering"});

        Rewriter rewriter(egraph, rewrites, config);
        rewriter.apply_rewrites();
        prune_symbolic_when_kernel_available();
    }

    ExtractionResult extract(Id target_id, const SizeBindings &bindings = {}) {
        if (config.enable_logging) {
            std::cout << "[API] Extracting concrete expression for target " << target_id << "...\n";
        }

        Extractor extractor(egraph, config);
        if (bindings.empty()) {
            return extractor.extract(target_id);
        } else {
            return extractor.extract(target_id, bindings);
        }
    }

    ExtractionResult extract_greedy(Id target_id, const SizeBindings &bindings = {}) {
        if (config.enable_logging) {
            std::cout << "[API] Fast greedy extraction for target " << target_id << "...\n";
        }

        Extractor extractor(egraph, config);
        return extractor.tree_extract(target_id, bindings);
    }

    std::vector<ExtractionResult> extract_symbolic() { return extract_symbolic(target_id); }

    std::vector<ExtractionResult> extract_symbolic(Id target_id) {
        if (config.enable_logging) {
            std::cout << "[API] Extracting symbolic expressions for target " << target_id << "...\n";
        }

        Extractor extractor(egraph, config);
        return extractor.extract_symbolic(target_id);
    }

    Expression optimize_concrete(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"complete"}) {
        if (config.enable_logging) {
            std::cout << "[API] Optimizing concrete expression: " << target_expr.to_string() << "\n";
        }
        initialize_config_for_expression(config, target_expr);
        Id target_id = add(target_expr);
        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite(rulesets);
        lower_to_kernels();
        return extract(target_id, {}).expr;
    }

    std::vector<double> evaluate_concrete(const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        return evaluate_concrete(target_id, size_bindings, bindings);
    }

    std::vector<double>
    evaluate_concrete(Id target_id, const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        auto start_evaluate = std::chrono::high_resolution_clock::now();
        auto result = extract(target_id, size_bindings);
        if (config.enable_logging) {
            std::cout << "[API] Extracted expression: " << result.expr.to_string() << "\n";
            std::cout << "[API] Evaluating concrete expression...\n";
            Dl_info info;
            if (dladdr(reinterpret_cast<void *>(cblas_dgemm), &info)) {
                std::cout << "[API] Active BLAS Kernel:   " << info.dli_fname << "\n";
            }
        }
        Evaluator evaluator(egraph, result, &size_bindings, bindings);

        auto result_eval = evaluator.evaluate();
        auto end_evaluate = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_evaluate - start_evaluate);
        std::cout << duration.count() << std::endl;
        return result_eval;
    }

    void optimize_symbolic(
        const Expression &target_expr, const std::vector<Expression> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"everything_but_lowering"}) {
        if (config.enable_logging) {
            std::cout << "[API] Optimizing symbolic expression: " << target_expr.to_string() << "\n";
            config.print_config();
        }
        initialize_config_for_expression(config, target_expr);

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
    Id get_target_id() const { return target_id; }
    EGraph &get_egraph() { return egraph; }
    MatrixProperty get_property() const {
        return std::get<MatrixProperty>(egraph.get_class_analysis_data(target_id).property);
    }
    MatrixProperty get_property(Id id) const {
        return std::get<MatrixProperty>(egraph.get_class_analysis_data(id).property);
    }
    void initialize_config(const Expression &expr) { initialize_config_for_expression(config, expr); }
    EGraphConfig config;

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

} // namespace egraph
