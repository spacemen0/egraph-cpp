#pragma once

#include "e_graph.h"
#include "expression.h"
#include "extractor.h"
#include "property_table.h"
#include "pruner.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace egraph {

class Expr {
  public:
    Expression ast;

    explicit Expr(Expression e) : ast(std::move(e)) {}

    explicit Expr(const std::string &name) : ast(name, {}) {}

    explicit Expr(double scalar_value) : ast(scalar_value, {}) {}
};

inline Expr operator+(const Expr &lhs, const Expr &rhs) { return Expr(Expression(Op::Add, {lhs.ast, rhs.ast})); }

inline Expr operator-(const Expr &lhs, const Expr &rhs) { return Expr(Expression(Op::Minus, {lhs.ast, rhs.ast})); }

inline Expr operator*(const Expr &lhs, const Expr &rhs) { return Expr(Expression(Op::Mul, {lhs.ast, rhs.ast})); }

inline Expr transpose(const Expr &e) { return Expr(Expression(Op::Tr, {e.ast})); }

inline Expr inverse(const Expr &e) { return Expr(Expression(Op::Inv, {e.ast})); }

inline Expr determinant(const Expr &e) { return Expr(Expression(Op::Det, {e.ast})); }

inline Expr log(const Expr &e) { return Expr(Expression(Op::Log, {e.ast})); }

// --- Optimization Context ---
class Context {
  public:
    EGraph egraph;

    void define_matrix(const std::string &name, int rows, int cols, const std::vector<std::string> &flags = {}) {
        MatrixProperty prop;
        prop.shape = Shape{rows, cols};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
    }

    void define_matrix_symbolic(
        const std::string &name, const std::string &rows_var, const std::string &cols_var,
        const std::vector<std::string> &flags = {}) {
        MatrixProperty prop;
        prop.shape = Shape{rows_var, cols_var};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
    }

    void define_matrix_symbolic(
        const std::string &name, const std::string &rows_var, int cols, const std::vector<std::string> &flags = {}) {
        MatrixProperty prop;
        prop.shape = Shape{rows_var, cols};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
    }

    void define_matrix_symbolic(
        const std::string &name, int rows, const std::string &cols_var, const std::vector<std::string> &flags = {}) {
        MatrixProperty prop;
        prop.shape = Shape{rows, cols_var};
        apply_flags(prop, flags);
        egraph.get_property_table().add_or_update_property_entry(name, prop);
    }

    Id add(const Expr &expr) { return egraph.add_expression(expr.ast); }

    void rewrite(
        const std::vector<std::string> &rulesets = {"complete"}, int max_nodes = 10000, bool enable_pruning = true,
        int max_iterations = 30) {
        std::vector<Rewrite> rewrites;
        for (const auto &ruleset : rulesets) {
            auto rules = get_rewrite_set_by_name(ruleset);
            rewrites.insert(rewrites.end(), rules.begin(), rules.end());
        }
        Rewriter rewriter(egraph, rewrites, max_nodes, enable_pruning);
        if (max_iterations > 0) {
            rewriter.apply_rewrites(max_iterations);
        } else {
            rewriter.apply_rewrites();
        }
    }

    void prune_symbolic_when_kernel_available() { Pruner::prune_symbolic_when_kernel_available(egraph); }

    Expression extract(Id target_id, const SizeBindings &bindings = {}) {
        CostStorage cost_storage(egraph);
        Extractor extractor(egraph, cost_storage);
        if (bindings.empty()) {
            return extractor.extract(target_id).expr;
        } else {
            return extractor.extract(target_id, bindings).expr;
        }
    }

    Expression optimize_concrete(
        const Expr &target_expr, const std::vector<Expr> &background_exprs = {},
        const std::vector<std::string> &rulesets = {"complete"}) {
        Id target_id = add(target_expr);
        for (const auto &bg_expr : background_exprs) {
            add(bg_expr);
        }
        rewrite(rulesets);
        rewrite({"lowering"});
        prune_symbolic_when_kernel_available();
        return extract(target_id);
    }

  private:
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

} // namespace egraph
