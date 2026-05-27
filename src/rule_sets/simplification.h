#pragma once
#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// Minus Eliminations
/// ----------------------------------------------------------
static const auto minus_cancel =
    make_rewrite("minus-cancel", "?a - ?a", "?__dynamic__", false, nullptr, [](EGraph &g, const Substitution &s) {
    return make_zero_for(g, s, "a");
});
static const auto add_comm_zero = make_rewrite("add-comm-zero", "?a + ?z", "?a", false, is_zero_cond("z"));
static const auto mul_zero_left =
    make_rewrite("mul-zero-left", "?z * ?a", "Dynamic", false, is_zero_cond("z"), [](EGraph &g, const Substitution &s) {
    const auto *z_prop = get_matrix_data(g, s.at("z"));
    const auto *a_prop = get_matrix_data(g, s.at("a"));
    return make_zero_of_shape(g, {z_prop->shape.first, a_prop->shape.second});
});
static const auto mul_zero_right = make_rewrite(
    "mul-zero-right", "?a * ?z", "Dynamic", false, is_zero_cond("z"), [](EGraph &g, const Substitution &s) {
    const auto *a_prop = get_matrix_data(g, s.at("a"));
    const auto *z_prop = get_matrix_data(g, s.at("z"));
    return make_zero_of_shape(g, {a_prop->shape.first, z_prop->shape.second});
});
static const auto scale_zero_scalar =
    make_rewrite("scale_zero_scalar", "Scale(?a, 0)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s) {
    const auto *a_prop = get_matrix_data(g, s.at("a"));
    return make_zero_of_shape(g, a_prop->shape);
});
static const auto scale_zero_matrix =
    make_rewrite("scale_zero_matrix", "Scale(?a, ?z)", "?a", false, is_zero_cond("a"));
/// Identity Eliminations
/// ----------------------------------------------------------
static const auto mul_identity_left = make_rewrite("mul-identity-left", "?a * ?i", "?a", false, is_identity_cond("i"));
static const auto mul_identity_right =
    make_rewrite("mul-identity-right", "?i * ?a", "?a", false, is_identity_cond("i"));
static const auto solve_by_id = make_rewrite("solve_by_id", "Sol(?b, ?a)", "?a", false, is_identity_cond("b"), nullptr);
static const auto scale_one = make_rewrite("scale_one", "Scale(?a, 1)", "?a");
/// Cancellations
/// ----------------------------------------------------------
static const auto invert_cancel_left = make_rewrite(
    "invert-cancel-left", "Inv(?a) * ?a", "?__dynamic__", false, nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto invert_cancel_right = make_rewrite(
    "invert-cancel-right", "?a * Inv(?a)", "?__dynamic__", false, nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto solve_cancel_left = make_rewrite("solve_cancel_left", "Sol(?a, ?a * ?b)", "?b");
static const auto solve_cancel_right = make_rewrite("solve_cancel_right", "?a * Sol(?a, ?b)", "?b");
static const auto solve_identity =
    make_rewrite("solve_identity", "Sol(?a, ?a)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto scale_collapse = make_rewrite(
    "scale-collapse", "Scale(Scale(?a, ?s1), ?s2)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s) {
    auto s1 = g.get_class_analysis_data(s.at("s1"));
    auto s2 = g.get_class_analysis_data(s.at("s2"));
    if (std::holds_alternative<double>(s1.property) && std::holds_alternative<double>(s2.property)) {
        double v = std::get<double>(s1.property) * std::get<double>(s2.property);
        std::string v_str = (v == static_cast<long long>(v)) ? std::to_string(static_cast<long long>(v)) : std::to_string(v);
        return g.add_expression(Expression("Scale(?a, " + v_str + ")"), s);
    }
    throw InvalidOperationError("scale_collapse requires both scale factors to be numbers");
});
static const auto scale_combine = make_rewrite(
    "scale_combine", "Scale(?a,?s1)+Scale(?a,?s2)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s) {
    auto s1 = g.get_class_analysis_data(s.at("s1"));
    auto s2 = g.get_class_analysis_data(s.at("s2"));
    if (std::holds_alternative<double>(s1.property) && std::holds_alternative<double>(s2.property)) {
        double v = std::get<double>(s1.property) + std::get<double>(s2.property);
        std::string v_str = (v == static_cast<long long>(v)) ? std::to_string(static_cast<long long>(v)) : std::to_string(v);
        return g.add_expression(Expression("Scale(?a, " + v_str + ")"), s);
    }
    throw std::runtime_error("scale_add: Expected both properties to be numbers");
});
static const auto scale_combine_implicit = make_rewrite(
    "scale_combine_implicit", "Scale(?a,?s1)+?a", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s) {
    auto s1 = g.get_class_analysis_data(s.at("s1"));
    if (std::holds_alternative<double>(s1.property)) {
        double v = std::get<double>(s1.property) + 1.0;
        std::string v_str = (v == static_cast<long long>(v)) ? std::to_string(static_cast<long long>(v)) : std::to_string(v);
        return g.add_expression(Expression("Scale(?a, " + v_str + ")"), s);
    }
    throw std::runtime_error("scale_add_implicit: Expected scale factor to be a number");
});

/// Property-Based Simplifications
/// ----------------------------------------------------------
static const auto orthogonal_transpose = make_rewrite(
    "orthogonal-transpose", "Tr(?a) * ?a", "Identity", false, is_orthogonal_cond("a"),
    [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto orthonormal_transpose = make_rewrite(
    "orthonormal-transpose", "Tr(?a) * ?a", "Identity", false, is_orthonormal_cond("a"),
    [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a", false);
});

static const std::vector<Rewrite> simplification_set = {
    minus_cancel,       add_comm_zero,       mul_zero_left,          mul_zero_right,       scale_zero_scalar,
    scale_zero_matrix,  mul_identity_left,   mul_identity_right,     solve_by_id,          scale_one,
    invert_cancel_left, invert_cancel_right, solve_cancel_left,      solve_cancel_right,   solve_identity,
    scale_collapse,     scale_combine,       scale_combine_implicit, orthogonal_transpose, orthonormal_transpose,
};