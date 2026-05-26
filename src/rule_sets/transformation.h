#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// Commutativity and Associativity
/// ----------------------------------------------------------
static const auto mul_assoc = make_rewrite("mul-assoc-left", "?a * (?b * ?c)", "(?a * ?b) * ?c", true);
static const auto commute_add = make_rewrite("commute-add", "?a + ?b", "?b + ?a");

/// Distributions and Normalizations
/// ----------------------------------------------------------
static const auto mul_distribute_left =
    make_rewrite("mul-distribute-over-add-left", "?a * (?b + ?c)", "?a * ?b + ?a * ?c", true);
static const auto mul_distribute_right =
    make_rewrite("mul-distribute-over-add-right", "(?a + ?b) * ?c", "?a * ?c + ?b * ?c", true);
static const auto sub_to_add_scale = make_rewrite("sub_to_add_scale", "?a - ?b", "?a + Scale(?b, -1)");

/// Transpositions and Inversions
/// ----------------------------------------------------------
static const auto invert_mat_prod = make_rewrite(
    "invert-mat-prod", "Inv(?a * ?b)", "Inv(?b) * Inv(?a)", true, [](const EGraph &g, const Substitution &s) {
    return is_non_singular_cond("a")(g, s) && is_non_singular_cond("b")(g, s);
});
static const auto mat_transpose_prod = make_rewrite("mat-transpose-prod", "Tr(?a * ?b)", "Tr(?b) * Tr(?a)", true);

/// Solver Restructuring
/// ----------------------------------------------------------
static const auto solve_composition = make_rewrite(
    "solve_composition", "Sol(?a * ?b, ?c)", "Sol(?b, Sol(?a, ?c))", true, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_square("b")(g, s);
});
static const auto inverse_solve =
    make_rewrite("inverse_solve", "Inv(Sol(?a, ?b))", "Sol(?b, ?a)", false, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_square("b")(g, s);
});