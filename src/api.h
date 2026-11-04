#pragma once

#include "e_graph.h"
#include "evaluator.h"
#include "expression.h"
#include "extractor.h"
#include "op_registry.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace egraph::EGraphRunner {

// Owns numerical values; later graph changes/evaluations do not invalidate them.
class EvaluationResults {
  public:
    EvaluationResults() = default;
    EvaluationResults(const EvaluationResults &) = default;
    EvaluationResults &operator=(const EvaluationResults &) = default;
    // MSVC's unordered_map move constructor can allocate and throw.
    EvaluationResults(EvaluationResults &&) noexcept(false) = default;
    EvaluationResults &operator=(EvaluationResults &&) noexcept(false) = default;

    const std::vector<double> &get(Id output) const { return values.at(output); }

  private:
    std::unordered_map<Id, std::vector<double>> values;
    friend class Context;
};

// --- Optimization Context ---
class Context {
  public:
    Context(EGraph egraph = EGraph(), EGraphConfig config = EGraphConfig())
        : egraph(std::move(egraph)), config(std::move(config)) {}

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

    Expression define_matrix(
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

    Expression define_matrix(
        const std::string &name, const std::string &rows_var, int cols, const std::vector<std::string> &flags = {}) {
        size_keys.push_back(rows_var);
        MatrixProperty prop;
        prop.shape = Shape{rows_var, cols};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    Expression define_matrix(
        const std::string &name, int rows, const std::string &cols_var, const std::vector<std::string> &flags = {}) {
        size_keys.push_back(cols_var);
        MatrixProperty prop;
        prop.shape = Shape{rows, cols_var};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
        return Expression(name);
    }

    //
    Id add(const Expression &expr) { return egraph.add_expression(expr); }

    Id add_output(const Expression &expr) {
        Id id = add(expr);
        outputs.emplace_back(id, expr);
        return id;
    }

    Op register_op(
        const std::string &name, bool is_kernel, AnalyzeFn analyze = nullptr, CostFn cost = nullptr,
        EvalFn eval = nullptr, std::optional<int> in_place_input = std::nullopt) {
        return OpRegistry::instance().register_op(
            name, is_kernel, std::move(analyze), std::move(cost), std::move(eval), in_place_input);
    }

    Op register_op(
        const std::string &name, bool is_kernel, AnalyzeFn analyze, CostFn cost, MatrixEvalFn eval,
        std::optional<int> in_place_input = std::nullopt) {
        return OpRegistry::instance().register_op(
            name, is_kernel, std::move(analyze), std::move(cost), std::move(eval), in_place_input);
    }

    Op register_kernel(
        const std::string &name, AnalyzeFn analyze = nullptr, CostFn cost = nullptr, EvalFn eval = nullptr,
        std::optional<int> in_place_input = std::nullopt) {
        return register_op(name, true, std::move(analyze), std::move(cost), std::move(eval), in_place_input);
    }

    Op register_kernel(
        const std::string &name, AnalyzeFn analyze, CostFn cost, MatrixEvalFn eval,
        std::optional<int> in_place_input = std::nullopt) {
        return register_op(name, true, std::move(analyze), std::move(cost), std::move(eval), in_place_input);
    }

    void rewrite(const std::vector<std::string> &rulesets = {"complete"}, int max_iterations = -1) {
        if (config.enable_logging) {
            std::cout << "[API] Running rewrite with rulesets: ";
            for (const auto &rs : rulesets)
                std::cout << rs << " ";
            std::cout << "\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);

        Rewriter rewriter(egraph, rewrites, config);
        if (max_iterations > 0) {
            rewriter.apply_rewrites(max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void apply_rewrites(const std::vector<Rewrite> &rewrites, int max_iterations = -1) {
        Rewriter rewriter(egraph, rewrites, config);
        if (max_iterations > 0) {
            rewriter.apply_rewrites(max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void prune_symbolic_when_kernel_available(const std::vector<Id> &roots = {}) {
        auto effective_roots = with_output_roots(roots);
        if (config.enable_logging) {
            std::cout << "[API] Pruning symbolic nodes\n";
        }
        auto res = Pruner::prune_symbolic_when_kernel_available(egraph, effective_roots);
        if (config.enable_logging) {
            std::cout << "[Pruner] Symbolic prune removed " << res.nodes_pruned << " nodes.\n";
            std::cout << "[API] EGraph Size after prune: " << egraph.num_nodes() << "\n";
        }
    }

    void rewrite_and_prune(
        const std::vector<Id> &target_ids, const std::vector<std::string> &rulesets = {"everything_but_lowering"},
        const std::function<void(int iteration)> &onIterationStart = nullptr,
        std::function<void(int iteration, const PruneResult &)> onIterationFinish = nullptr) {
        if (config.enable_logging) {
            std::cout << "[API] Starting rewrite_and_prune...\n";
        }

        if (!onIterationFinish && config.enable_logging) {
            onIterationFinish = [](int iteration, const PruneResult &result) {
                std::cout << "[Pruner] Iteration " << iteration + 1 << " finished. Pruned " << result.nodes_pruned
                          << " nodes.\n";
            };
        }

        std::vector<Rewrite> rewrites = build_rewrite_sets(rulesets);
        Rewriter rewriter(egraph, rewrites, config);
        Extractor extractor(egraph, config);
        Pruner pruner(egraph, extractor, config);

        pruner.rewrite_and_prune(
            with_output_roots(target_ids), rewriter, size_keys, onIterationStart, onIterationFinish);
    }

    void lower_to_kernels(const std::vector<Id> &roots = {}) {
        auto effective_roots = with_output_roots(roots);
        if (!effective_roots.empty()) {
            Pruner::eliminate_unreachable_classes(egraph, effective_roots);
        }
        if (config.enable_logging) {
            std::cout << "[API] Lowering to kernels...\n";
            std::cout << "[API] EGraph Size before lowering: " << egraph.num_nodes() << "\n";
        }
        std::vector<Rewrite> rewrites = build_rewrite_sets({"lowering"});
        auto new_config = config;
        new_config.rewrite.enable_backoff = false;
        new_config.rewrite.enable_node_limit = false;
        Rewriter rewriter(egraph, rewrites, new_config);
        rewriter.apply_rewrites();
        prune_symbolic_when_kernel_available(effective_roots);
    }

    ExtractionResult extract(Id target_id, const SizeBindings &bindings) {
        if (config.enable_logging) {
            std::cout << "[API] Extracting concrete expression for target " << target_id << "...\n";
        }

        Extractor extractor(egraph, config);
        return extractor.extract(target_id, bindings);
    }

    ExtractionResult extract_greedy(Id target_id, const SizeBindings &bindings) {
        if (config.enable_logging) {
            std::cout << "[API] Fast greedy extraction for target " << target_id << "...\n";
        }

        Extractor extractor(egraph, config);
        return extractor.tree_extract(target_id, bindings);
    }

    // Optimize all registered outputs; none is an implicit default target.
    void optimize(const std::vector<std::string> &rulesets = {"everything_but_lowering"}) {
        require_outputs();
        const auto &largest = *std::ranges::max_element(outputs, [](const auto &a, const auto &b) {
            return a.second.node_count() < b.second.node_count();
        });
        initialize_config_for_expression(config, largest.second);
        restore_outputs();
        rewrite_and_prune({}, rulesets, [&](int iteration) {
            if (iteration > 0) {
                restore_outputs();
            }
        });
    }

    // Extend the first output's plan with any missing outputs, keeping existing
    // choices for shared classes. This reuses computation without joint optimization.
    EvaluationResults evaluate(const SizeBindings &size_bindings, const DataBindings &bindings = {}) {
        require_outputs();
        auto start_extraction = std::chrono::high_resolution_clock::now();
        auto result = extract(outputs.front().first, size_bindings);

        std::vector<Id> roots;
        for (const auto &[id, expr] : outputs) {
            Id root = egraph.find_class_id(id);
            roots.push_back(root);
            if (!result.choices.contains(root)) {
                auto additional = extract(root, size_bindings);
                result.choices.insert(additional.choices.begin(), additional.choices.end());
            }
        }
        // As in the previous evaluator, visit additional roots before the first output.
        std::ranges::rotate(roots, roots.begin() + 1);
        Extractor extractor(egraph, config);
        result.execution_order = extractor.build_execution_order(roots, result.choices);

        Evaluator evaluator(egraph, result, size_bindings, bindings, roots);
        if (config.enable_logging) {
            print_execution_plan(result, &bindings);
        }
        auto start_evaluate = std::chrono::high_resolution_clock::now();
        evaluator.evaluate();
        auto end_evaluate = std::chrono::high_resolution_clock::now();
        last_extraction_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(start_evaluate - start_extraction);
        last_evaluation_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_evaluate - start_evaluate);

        EvaluationResults results;
        for (const auto &[id, expr] : outputs) {
            results.values.emplace(id, evaluator.get_preserved(egraph.find_class_id(id)));
        }
        return results;
    }

    void print_properties() const { egraph.get_property_table().print_all_properties(); }

    std::optional<Id> find_expr(const Expression &expr) const { return egraph.find_expression_id(expr); }

    void clear() {
        egraph = EGraph();
        outputs.clear();
        size_keys.clear();
        last_extraction_duration = std::chrono::microseconds{0};
        last_evaluation_duration = std::chrono::microseconds{0};
    }
    EGraph &get_egraph() { return egraph; }
    MatrixProperty get_property(Id id) const {
        return std::get<MatrixProperty>(egraph.get_class_analysis_data(egraph.find_class_id(id)).property);
    }
    void initialize_config(const Expression &expr) { initialize_config_for_expression(config, expr); }

    std::chrono::microseconds get_last_extraction_duration() const { return last_extraction_duration; }
    std::chrono::microseconds get_last_evaluation_duration() const { return last_evaluation_duration; }
    std::chrono::microseconds get_last_total_duration() const {
        return last_extraction_duration + last_evaluation_duration;
    }
    double get_last_extraction_duration_ms() const {
        return static_cast<double>(last_extraction_duration.count()) / 1000.0;
    }
    double get_last_evaluation_duration_ms() const {
        return static_cast<double>(last_evaluation_duration.count()) / 1000.0;
    }
    double get_last_total_duration_ms() const {
        return static_cast<double>((last_extraction_duration + last_evaluation_duration).count()) / 1000.0;
    }

  private:
    EGraphConfig config;
    EGraph egraph;
    // Retain the original syntax for reinsertion between optimization iterations.
    // IDs belong to this graph and must not be reused after clear().
    std::vector<std::pair<Id, Expression>> outputs;
    std::vector<std::string> size_keys;
    std::chrono::microseconds last_extraction_duration{0};
    std::chrono::microseconds last_evaluation_duration{0};

    void require_outputs() const {
        if (outputs.empty()) {
            throw std::logic_error("No outputs registered; call add_output first");
        }
    }

    std::vector<Id> with_output_roots(const std::vector<Id> &roots) const {
        std::vector<Id> result = roots;
        for (const auto &[id, expr] : outputs) {
            result.push_back(id);
        }
        for (auto &root : result) {
            root = egraph.find_class_id(root);
        }
        std::ranges::sort(result);
        result.erase(std::ranges::unique(result).begin(), result.end());
        return result;
    }

    void restore_outputs() {
        for (const auto &[id, expr] : outputs) {
            Id restored = egraph.add_expression(expr);
            egraph.union_classes(id, restored);
        }
        egraph.rebuild();
    }

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

} // namespace egraph::EGraphRunner
