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

/// Identity Eliminations
/// ----------------------------------------------------------
static const auto mul_identity_left = make_rewrite("mul-identity-left", "?a * ?i", "?a", false, is_identity_cond("i"));
static const auto mul_identity_right =
    make_rewrite("mul-identity-right", "?i * ?a", "?a", false, is_identity_cond("i"));
static const auto solve_by_id = make_rewrite("solve_by_id", "Sol(?b, ?a)", "?a", false, is_identity_cond("b"), nullptr);

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